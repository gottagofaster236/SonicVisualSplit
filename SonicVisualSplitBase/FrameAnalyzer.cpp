#include "FrameAnalyzer.h"
#include "TimeRecognizer.h"
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

    cv::UMat frame = getSavedFrame(frameTime);
    if (frame.cols == 0 || frame.rows == 0) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
        return result;
    }
    
    std::vector<TimeRecognizer::Match> allMatches;
    if (!checkIfFrameIsSingleColor(frame)) {
        TimeRecognizer& timeRecognizer = TimeRecognizer::getInstance(gameName, templatesDirectory, isComposite);
        allMatches = timeRecognizer.recognizeTime(frame, checkForScoreScreen, result);
    }

    if (visualize)
        visualizeResult(allMatches);
    return result;
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



cv::UMat FrameAnalyzer::getSavedFrame(long long frameTime) {
    originalFrame = FrameStorage::getSavedFrame(frameTime);

    // Make sure the frame isn't too large (that'll slow down the calculations).
    if (originalFrame.rows > TimeRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double) TimeRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) / originalFrame.rows;
        cv::resize(originalFrame, originalFrame, {}, scaleFactor, scaleFactor, cv::INTER_AREA);
    }

    cv::UMat frame = originalFrame;
    if (isStretchedTo16By9)
        cv::resize(frame, frame, {}, scaleFactorTo4By3, 1, cv::INTER_AREA);
    return frame;
}


bool FrameAnalyzer::checkIfFrameIsSingleColor(cv::UMat frame) {
    // Checking just the rectangle with the digits (with doubled height to check more). 
    const std::unique_ptr<TimeRecognizer>& instance = TimeRecognizer::getCurrentInstance();
    if (!instance)
        return false;
    cv::Rect2f relativeDigitsRect = instance->getRelativeDigitsRect();
    cv::Rect digitsRect = {(int) (relativeDigitsRect.x * frame.cols), (int) (relativeDigitsRect.y * frame.rows),
        (int) (relativeDigitsRect.width * frame.cols), (int) (relativeDigitsRect.height * frame.rows)};

    cv::Rect checkRect = digitsRect;
    checkRect.height *= 2;
    checkRect.height = std::min(checkRect.height, frame.rows - checkRect.y);
    if (checkRect.empty())
        return false;
    frame = frame(checkRect);

    const cv::Scalar black(0, 0, 0), white(255, 255, 255);
    if (checkIfImageIsSingleColor(frame, black, 40)) {
        result.isBlackScreen = true;
        return true;
    }
    else if (checkIfImageIsSingleColor(frame, white, 25)) {
        result.isWhiteScreen = true;
        return true;
    }
    else if (gameName == "Sonic 2") {
        /* Sonic 2 has a transition where it shows the act name in front of a red/blue background.
         * We just check that there's no white on the screen. */
        cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        double maxBrightness;
        cv::minMaxLoc(frame, nullptr, &maxBrightness);
        if (maxBrightness < 120) {
            result.isBlackScreen = true;
            return true;
        }
    }

    return false;
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


void FrameAnalyzer::visualizeResult(const std::vector<TimeRecognizer::Match>& allMatches) {
    originalFrame.copyTo(result.visualizedFrame);
    int lineThickness = 1 + result.visualizedFrame.rows / 500;
    for (auto match : allMatches) {
        if (isStretchedTo16By9) {
            match.location.x /= scaleFactorTo4By3;
            match.location.width /= scaleFactorTo4By3;
        }
        cv::rectangle(result.visualizedFrame, match.location, cv::Scalar(0, 0, 255), lineThickness);
    }
}

}  // namespace SonicVisualSplitBase
