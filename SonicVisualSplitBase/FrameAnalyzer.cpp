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

static std::recursive_mutex frameAnalyzationMutex;

static const double scaleFactorTo4By3 = (4 / 3.) / (16 / 9.);


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
        bool isStretchedTo16By9, bool isComposite) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalyzationMutex);

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
    std::lock_guard<std::recursive_mutex> guard(frameAnalyzationMutex);

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
        return result;
    else if (visualize)
        visualizeResult();

    if (digitsRecognizer.recalculatedDigitsPlacementLastTime() && !result.recognizedTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        DigitsRecognizer::resetDigitsPlacement();
    }
    return result;
}


void FrameAnalyzer::checkRecognizedSymbols(bool checkForScoreScreen, bool visualize) {
    result.recognizedTime = false;

    std::map<char, std::vector<cv::Rect2f>> scoreAndTimePositions;

    if (checkForScoreScreen) {
        doCheckForScoreScreen(scoreAndTimePositions);
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
            if (interval < 28) {
                timeDigits.erase(timeDigits.begin() + i + 1);
                i--;
            }
        }
        else {
            // Checking that the interval is not too high.
            if (interval > 20) {
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

    if (visualize) {
        recognizedSymbols = timeDigits;
        char additionalSymbols[] = {DigitsRecognizer::SCORE, DigitsRecognizer::TIME};
        for (char c : additionalSymbols) {
            for (const cv::Rect2f& position : scoreAndTimePositions[c]) {
                recognizedSymbols.push_back({position, c});
            }
        }
        visualizeResult();
    }
}


void FrameAnalyzer::doCheckForScoreScreen(std::map<char, std::vector<cv::Rect2f>>& scoreAndTimePositions) {
    for (const auto& match : recognizedSymbols) {
        if (match.symbol == DigitsRecognizer::SCORE || match.symbol == DigitsRecognizer::TIME) {
            scoreAndTimePositions[match.symbol].push_back(match.location);
        }
    }

    if (scoreAndTimePositions[DigitsRecognizer::SCORE].empty() || scoreAndTimePositions[DigitsRecognizer::TIME].empty()) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    std::vector<cv::Rect2f>& timeRects = scoreAndTimePositions[DigitsRecognizer::TIME];
    cv::Rect2f timeRect = *std::ranges::min_element(timeRects, [](const auto& rect1, const auto& rect2) {
        return rect1.y < rect2.y;
    });

    if (timeRects.size() >= 2) {
        // Make sure that the other recognized TIME rectangles are valid.
        for (int i = (int) timeRects.size() - 1; i >= 0; i--) {
            if (timeRects[i] == timeRect)
                continue;
            if (timeRects[i].y - timeRect.y < timeRect.width || timeRects[i].y > 3 * originalFrame.rows / 4) {
                timeRects.erase(timeRects.begin() + i);
            }
        }
    }

    /* There's always a "TIME" label on top of the screen.
     * But there's a second one during the score countdown screen ("TIME BONUS"). */
    result.isScoreScreen = (timeRects.size() >= 2);
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
    /* Checking the rectangle with the digits to see whether */
    const std::unique_ptr<DigitsRecognizer>& instance = DigitsRecognizer::getCurrentInstance();
    if (!instance)
        return SingleColor::NOT_SINGLE_COLOR;
    cv::Rect2f relativeDigitsRect = instance->getRelativeDigitsRect();
    cv::Rect digitsRect = {(int) (relativeDigitsRect.x * frame.cols), (int) (relativeDigitsRect.y * frame.rows),
        (int) (relativeDigitsRect.width * frame.cols), (int) (relativeDigitsRect.height * frame.rows)};
    if (digitsRect.empty())
        return SingleColor::NOT_SINGLE_COLOR;

    frame = frame(digitsRect);

    const cv::Scalar black(0, 0, 0), white(255, 255, 255), red(0, 0, 255), blue(198, 65, 33);
    if (checkIfImageIsSingleColor(frame, black)) {
        return SingleColor::BLACK;
    }
    else if (checkIfImageIsSingleColor(frame, white)) {
        return SingleColor::WHITE;
    }
    else if (gameName == "Sonic 2" &&
            (checkIfImageIsSingleColor(frame, red) || checkIfImageIsSingleColor(frame, blue))) {
        // This is a screen with the act name, we call it a frame of the black transition to simplify the code.
        return SingleColor::BLACK;
    }

    return SingleColor::NOT_SINGLE_COLOR;
}


bool FrameAnalyzer::checkIfImageIsSingleColor(cv::UMat img, cv::Scalar color) {
    /* Converting to a wider type to make sure no overflow happens during subtraction,
     * then calculating the L2 norm between the image and the color. */
    img.convertTo(img, CV_32SC3);
    cv::subtract(img, color, img);
    double squareDifference = cv::norm(img, cv::NORM_L2SQR);
    double avgSquareDifference = squareDifference / img.total();
    const int MAX_AVG_DIFFERENCE = 25;
    return avgSquareDifference < MAX_AVG_DIFFERENCE * MAX_AVG_DIFFERENCE * 3;  // Three channels, so multiplying by 3.
}


void FrameAnalyzer::reportCurrentSplitIndex(int currentSplitIndex) {
    FrameAnalyzer::currentSplitIndex = currentSplitIndex;
}


int FrameAnalyzer::getCurrentSplitIndex() {
    return currentSplitIndex;
}


void FrameAnalyzer::lockFrameAnalyzationMutex() {
    frameAnalyzationMutex.lock();
}


void FrameAnalyzer::unlockFrameAnalyzationMutex() {
    frameAnalyzationMutex.unlock();
}

}  // namespace SonicVisualSplitBase
