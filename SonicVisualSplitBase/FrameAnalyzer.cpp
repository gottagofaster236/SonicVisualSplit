﻿#include "FrameAnalyzer.h"
#include "TimeRecognizer.h"
#include "FrameStorage.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cctype>
#include <opencv2/imgcodecs.hpp>


namespace SonicVisualSplitBase {

static std::recursive_mutex frameAnalysisMutex;

static const double scaleFactorTo4By3 = (4 / 3.) / (16 / 9.);


FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings) :
        settings(settings), timeRecognizer(settings) {
    resetTemplate = settings.loadTemplateImageFromFile('R');
    // Removing the alpha channel.
    cv::cvtColor(resetTemplate, resetTemplate, cv::COLOR_BGRA2BGR);
}


AnalysisResult FrameAnalyzer::analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalysisMutex);

    result = AnalysisResult();
    result.frameTime = frameTime;
    result.recognizedTime = false;

    cv::UMat originalFrame = getSavedFrame(frameTime);
    if (originalFrame.cols == 0 || originalFrame.rows == 0) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
        return result;
    }
    cv::UMat frame = fixAspectRatioIfNeeded(originalFrame);
    bool resetScreenCheckResult = checkForResetScreen(frameTime);
    
    std::vector<TimeRecognizer::Match> allMatches;
    if (!checkIfFrameIsSingleColor(frame)) {
        allMatches = timeRecognizer.recognizeTime(frame, checkForScoreScreen, result);
    }

    if (visualize)
        visualizeResult(allMatches, originalFrame);
    return result;
}


void FrameAnalyzer::resetDigitsPositions() {
    timeRecognizer.resetDigitsPositions();
}


void FrameAnalyzer::reportCurrentSplitIndex(int currentSplitIndex) {
    timeRecognizer.reportCurrentSplitIndex(currentSplitIndex);
}


bool FrameAnalyzer::checkForResetScreen(long long frameTime) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalysisMutex);

    double bestScale = timeRecognizer.getBestScale();
    if (bestScale == -1)
        return false;

    cv::UMat frame = getSavedFrame(frameTime);
    frame = fixAspectRatioIfNeeded(frame);
    cv::resize(frame, frame, {}, bestScale, bestScale, cv::INTER_AREA);
    cv::Rect timeRect = timeRecognizer.getTimeRectForFrameSize(frame.size());
    int height = timeRect.height;

    cv::Rect gameScreenRect = {
        (int) (timeRect.x - height / 11. * 17.),
        (int) (timeRect.y - height / 11. * 25.),
        (int) (height / 11. * 320.),
        (int) (height / 11. * 224.)
    };
    gameScreenRect &= cv::Rect({}, frame.size());
    if (gameScreenRect.empty())
        return false;

    cv::UMat gameScreen = frame(gameScreenRect);
    cv::resize(gameScreen, gameScreen, resetTemplate.size(), 0, 0, cv::INTER_AREA);

    // Cropping the reset template
    const double borderRatio = 0.1;
    cv::Size borderSize(
        (int) (resetTemplate.cols * borderRatio), 
        (int) (resetTemplate.rows * borderRatio)
    );
    cv::Rect cropRect(borderSize, resetTemplate.size() - borderSize * 2);
    cv::UMat gameScreenCropped = gameScreen(cropRect);
    cv::UMat resetTemplateCropped = resetTemplate(cropRect);

    cv::UMat result;
    cv::matchTemplate(gameScreen, resetTemplateCropped, result, cv::TM_SQDIFF);
    double squareDifference;
    cv::minMaxLoc(result, &squareDifference);
    double avgSquareDifference = squareDifference / gameScreen.total();
    const double maxAvgDifference = 35;  // Allowing the color to deviate by 40 (out of 255).
    return avgSquareDifference < maxAvgDifference * maxAvgDifference * 3;  // Three channels, so multiplying by 3.
}


void FrameAnalyzer::lockFrameAnalysisMutex() {
    frameAnalysisMutex.lock();
}


void FrameAnalyzer::unlockFrameAnalysisMutex() {
    frameAnalysisMutex.unlock();
}


cv::UMat FrameAnalyzer::getSavedFrame(long long frameTime) {
    cv::UMat frame = FrameStorage::getSavedFrame(frameTime);

    // Make sure the frame isn't too large (that'll slow down the calculations).
    if (frame.rows > TimeRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double) TimeRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) / frame.rows;
        cv::resize(frame, frame, {}, scaleFactor, scaleFactor, cv::INTER_AREA);
    }

    return frame;
}


cv::UMat FrameAnalyzer::fixAspectRatioIfNeeded(cv::UMat frame) {
    if (settings.isStretchedTo16By9 && !frame.empty())
        cv::resize(frame, frame, {}, scaleFactorTo4By3, 1, cv::INTER_AREA);
    return frame;
}


bool FrameAnalyzer::checkIfFrameIsSingleColor(cv::UMat frame) {
    // Checking a rectangle near the digits rectangle.
    if (timeRecognizer.getTimeSinceDigitsPositionsLastUpdated() > std::chrono::seconds(10)) {
        // The rectangle is outdated now.
        return false;
    }
    cv::Rect timeRect = timeRecognizer.getTimeRectForFrameSize(frame.size());
    cv::Rect checkRect = timeRect;
    // Extending the checked area.
    checkRect.width += timeRect.height * 3;
    checkRect.height *= 2;
    checkRect &= cv::Rect({0, 0}, frame.size());  // Make sure checkRect is not out of bounds.
    if (checkRect.empty())
        return false;
    frame = frame(checkRect);

    const cv::Scalar black(0, 0, 0), white(255, 255, 255);
    const double maxAvgDifference = 40;  // Allowing the color to deviate by 40 (out of 255).

    if (checkIfImageIsSingleColor(frame, black, maxAvgDifference)) {
        result.isBlackScreen = true;
        return true;
    }
    else if (checkIfImageIsSingleColor(frame, white, maxAvgDifference)) {
        result.isWhiteScreen = true;
        return true;
    }
    else if (settings.gameName == "Sonic 2") {
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


void FrameAnalyzer::visualizeResult(
        const std::vector<TimeRecognizer::Match>& allMatches, cv::UMat originalFrame) {
    originalFrame.copyTo(result.visualizedFrame);
    int lineThickness = 1 + result.visualizedFrame.rows / 500;
    for (auto match : allMatches) {
        if (settings.isStretchedTo16By9) {
            match.location.x /= scaleFactorTo4By3;
            match.location.width /= scaleFactorTo4By3;
        }
        cv::rectangle(result.visualizedFrame, match.location, cv::Scalar(0, 0, 255), lineThickness);
    }
}

}  // namespace SonicVisualSplitBase
