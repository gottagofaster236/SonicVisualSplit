#include "FrameAnalyzer.h"
#include "DigitsRecognizer.h"
#include "FrameStorage.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cctype>


namespace SonicVisualSplitBase {

static std::recursive_mutex frameAnalysisMutex;

static const double scaleFactorTo4By3 = (4 / 3.) / (16 / 9.);


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
        bool isStretchedTo16By9, bool isComposite) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalysisMutex);

    if (!instance || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory
            || instance->isStretchedTo16By9 != isStretchedTo16By9 || instance->isComposite != isComposite) {
        instance = std::unique_ptr<FrameAnalyzer>(
            new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9, isComposite));
    }
    return *instance;
}


FrameAnalyzer::FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory,
        bool isStretchedTo16By9, bool isComposite)
    : gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9), isComposite(isComposite) {}


AnalysisResult FrameAnalyzer::analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalysisMutex);

    result = AnalysisResult();
    result.frameTime = frameTime;
    result.recognizedTime = false;

    originalFrame = FrameStorage::getSavedFrame(frameTime);
    if (originalFrame.cols == 0 || originalFrame.rows == 0) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
        return result;
    }

    // Make sure the frame isn't too large (that'll slow down the calculations).
    if (originalFrame.rows > DigitsRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double) DigitsRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) / originalFrame.rows;
        cv::resize(originalFrame, originalFrame, cv::Size(), scaleFactor, scaleFactor, cv::INTER_AREA);
    }

    cv::UMat frame = originalFrame;
    if (isStretchedTo16By9) {
        cv::resize(frame, frame, cv::Size(), scaleFactorTo4By3, 1, cv::INTER_AREA);
    }
    if (visualize) {
        originalFrame.copyTo(result.visualizedFrame);
    }
    switch (checkIfFrameIsSingleColor(frame)) {
    case SingleColor::BLACK:
        result.isBlackScreen = true;
        return result;
    case SingleColor::WHITE:
        result.isWhiteScreen = true;
        return result;
    }

    DigitsRecognizer& digitsRecognizer = DigitsRecognizer::getInstance(gameName, templatesDirectory, isComposite);
    recognizedSymbols = digitsRecognizer.findLabelsAndDigits(frame, checkForScoreScreen);
    checkRecognizedSymbols(checkForScoreScreen, visualize);
    originalFrame.release();

    if (result.recognizedTime)
        digitsRecognizer.reportRecognitionSuccess();
    else
        digitsRecognizer.reportRecognitionFailure();

    if (visualize)
        visualizeResult();
    return result;
}


void FrameAnalyzer::checkRecognizedSymbols(bool checkForScoreScreen, bool visualize) {
    result.recognizedTime = false;

    std::vector<DigitsRecognizer::Match> labels;

    if (checkForScoreScreen) {
        doCheckForScoreScreen(labels);
        if (result.errorReason != ErrorReasonEnum::NO_ERROR)
            return;
    }

    std::vector<DigitsRecognizer::Match> timeDigits;
    for (const auto& match : recognizedSymbols) {
        if (std::isdigit(match.symbol))
            timeDigits.push_back(match);
    }
    std::ranges::sort(timeDigits, {}, [](const auto& match) {
        return match.location.x;
    });

    bool includesMilliseconds = (gameName == "Sonic CD");
    int requiredDigitsCount;
    if (includesMilliseconds)
        requiredDigitsCount = 5;
    else
        requiredDigitsCount = 3;

    for (int i = 0; i + 1 < timeDigits.size(); i++) {
        double bestScale = DigitsRecognizer::getCurrentInstance()->getBestScale();
        const cv::Rect2f& prevLocation = timeDigits[i].location, nextLocation = timeDigits[i + 1].location;
        double interval = ((nextLocation.x + nextLocation.width) -
            (prevLocation.x + prevLocation.width)) * bestScale;
        
        if (i == 0 || i == 2) {
            /* Checking that separators (:, ' and ") aren't detected as digits.
             * For that we check that the digits adjacent to the separator have increased interval. */
            if (interval < 26.5) {
                timeDigits.erase(timeDigits.begin() + i + 1);
                i--;
            }
        }
        else {
            // Checking that the interval is not too high.
            if (interval > 20.5) {
                result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
                return;
            }
        }
    }

    if (isComposite) {
        while (timeDigits.size() > requiredDigitsCount && timeDigits.back().symbol == '1')
            timeDigits.pop_back();
    }

    if (timeDigits.size() != requiredDigitsCount) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }

    std::string timeDigitsStr;
    for (const auto& match : timeDigits)
        timeDigitsStr += match.symbol;

    // Formatting the timeString and calculating timeInMilliseconds
    std::string minutes = timeDigitsStr.substr(0, 1), seconds = timeDigitsStr.substr(1, 2);
    if (std::stoi(seconds) >= 60) {
        // The number of seconds is always 59 or lower.
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    result.timeString = minutes + "'" + seconds;
    result.timeInMilliseconds = std::stoi(minutes) * 60 * 1000 + std::stoi(seconds) * 1000;
    if (includesMilliseconds) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeString += '"' + milliseconds;
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;
    }

    result.recognizedTime = true;
    result.errorReason = ErrorReasonEnum::NO_ERROR;

    recognizedSymbols = std::move(timeDigits);
    recognizedSymbols.insert(recognizedSymbols.end(), labels.begin(), labels.end());
}


void FrameAnalyzer::doCheckForScoreScreen(std::vector<DigitsRecognizer::Match>& labels) {
    std::vector<DigitsRecognizer::Match> timeMatches;
    std::vector<DigitsRecognizer::Match> scoreMatches;
    for (const auto& match : recognizedSymbols) {
        if (match.symbol == DigitsRecognizer::TIME)
            timeMatches.push_back(match);
        else if (match.symbol == DigitsRecognizer::SCORE)
            scoreMatches.push_back(match);
    }

    if (timeMatches.empty() || scoreMatches.empty()) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    auto topTimeLabel = std::ranges::min(timeMatches, {}, [](const auto& timeMatch) {
        return timeMatch.location.y;
    });

    // Make sure that the other TIME matches are valid.
    std::erase_if(timeMatches, [&](const auto& timeMatch) {
        if (timeMatch == topTimeLabel)
            return false;
        const cv::Rect& topLocation = topTimeLabel.location;
        const cv::Rect& otherLocation = timeMatch.location;
        return (otherLocation.y - topLocation.y < topLocation.width
            || otherLocation.y > 3 * originalFrame.rows / 4);
    });

    /* There's always a "TIME" label on top of the screen.
     * But there's a second one during the score countdown screen ("TIME BONUS"). */
    result.isScoreScreen = (timeMatches.size() >= 2);

    labels = std::move(timeMatches);
    labels.insert(labels.end(), scoreMatches.begin(), scoreMatches.end());
}


void FrameAnalyzer::visualizeResult() {
    int lineThickness = 1 + result.visualizedFrame.rows / 500;
    for (auto match : recognizedSymbols) {
        if (isStretchedTo16By9) {
            match.location.x /= scaleFactorTo4By3;
            match.location.width /= scaleFactorTo4By3;
        }
        cv::rectangle(result.visualizedFrame, match.location, cv::Scalar(0, 0, 255), lineThickness);
    }
}


FrameAnalyzer::SingleColor FrameAnalyzer::checkIfFrameIsSingleColor(cv::UMat frame) {
    // Checking just the rectangle with the digits (with doubled height to check more). 
    const std::unique_ptr<DigitsRecognizer>& instance = DigitsRecognizer::getCurrentInstance();
    if (!instance)
        return SingleColor::NOT_SINGLE_COLOR;
    cv::Rect2f relativeDigitsRect = instance->getRelativeDigitsRect();
    cv::Rect digitsRect = {(int) (relativeDigitsRect.x * frame.cols), (int) (relativeDigitsRect.y * frame.rows),
        (int) (relativeDigitsRect.width * frame.cols), (int) (relativeDigitsRect.height * frame.rows)};

    cv::Rect checkRect = digitsRect;
    checkRect.height *= 2;
    checkRect.height = std::min(checkRect.height, frame.rows - checkRect.y);
    if (checkRect.empty())
        return SingleColor::NOT_SINGLE_COLOR;
    frame = frame(checkRect);

    const cv::Scalar black(0, 0, 0), white(255, 255, 255);
    if (checkIfImageIsSingleColor(frame, black, 40)) {
        return SingleColor::BLACK;
    }
    else if (checkIfImageIsSingleColor(frame, white, 25)) {
        return SingleColor::WHITE;
    }
    else if (gameName == "Sonic 2") {
        /* Sonic 2 has a transition where it shows the act name in front of a red/blue background.
         * We just check that there's no white on the screen. */
        cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        double maxBrightness;
        cv::minMaxLoc(frame, nullptr, &maxBrightness);
        if (maxBrightness < 120)
            return SingleColor::BLACK;
    }

    return SingleColor::NOT_SINGLE_COLOR;
}


bool FrameAnalyzer::checkIfImageIsSingleColor(cv::UMat img, cv::Scalar color, double maxAvgDifference) {
    /* Converting to a wider type to make sure no overflow happens during subtraction,
     * then calculating the L2 norm between the image and the color. */
    img.convertTo(img, CV_32SC3);
    cv::subtract(img, color, img);
    double squareDifference = cv::norm(img, cv::NORM_L2SQR);
    double avgSquareDifference = squareDifference / img.total();
    return avgSquareDifference < maxAvgDifference * maxAvgDifference * 3;  // Three channels, so multiplying by 3.
}


void FrameAnalyzer::reportCurrentSplitIndex(int currentSplitIndex) {
    FrameAnalyzer::currentSplitIndex = currentSplitIndex;
}


int FrameAnalyzer::getCurrentSplitIndex() {
    return currentSplitIndex;
}


void FrameAnalyzer::lockFrameAnalysisMutex() {
    frameAnalysisMutex.lock();
}


void FrameAnalyzer::unlockFrameAnalysisMutex() {
    frameAnalysisMutex.unlock();
}

}  // namespace SonicVisualSplitBase
