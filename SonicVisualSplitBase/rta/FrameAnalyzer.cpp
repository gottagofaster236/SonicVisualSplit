#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <vector>
#include <algorithm>

namespace SonicVisualSplitBase {
namespace RTA {

// Avoid several consecutive splits at once
constexpr long long WAIT_AFTER_SPLIT_MS = 3000;

using VideoCaptureManager::CapturedFrame;

FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback) :
    settings(settings), callback(callback), onFrameCapturedListener(*this), gameRectTimestamp(0), 
    lastSplitTime(0), splitIndex(0) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}

FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
}


cv::Mat FrameAnalyzer::visualizeLastFrame() {
    /*
    cv::UMat visualizedFrame;
    frame.copyTo(visualizedFrame);
    int lineThickness = 1 + frame.rows / 500;
    cv::rectangle(visualizedFrame, gameRect, cv::Scalar(0, 0, 255), lineThickness);
    cv::drawContours(visualizedFrame, contours, -1, cv::Scalar(255, 0, 0), lineThickness);
    cv::imwrite("C:\\tmp\\" + std::to_string(currentFrame.timestamp) + "contours.png", visualizedFrame);
    */
    return cv::Mat();  // TODO
}

void FrameAnalyzer::onReset() {
    std::lock_guard guard(analysisMutex);
    splitIndex = 0;
    lastSplitTime = VideoCaptureManager::getCurrentTimeInMilliseconds();
}


static bool isTitleScreen(const cv::UMat& gameRect);

void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }

    {
        std::lock_guard guard(analysisMutex);
        if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
    }

    cv::Rect gameRectOnFade = detectGameRectOnFade(currentFrame, previousFrame);

    bool haveToDetectTitleScreen = false;
    if (!gameRectOnFade.empty()) {
        std::lock_guard guard(analysisMutex);
        if (currentFrame.timestamp - gameRectTimestamp >= WAIT_AFTER_SPLIT_MS) {
            gameRect = gameRectOnFade;
            gameRectTimestamp = currentFrame.timestamp;
        }
        haveToDetectTitleScreen = splitIndex == 0;
    } else {
        // TODO: reset detection against previously computed game rect here
        return;
    }

    if (haveToDetectTitleScreen) {
        if (!isTitleScreen(previousFrame.frame(gameRectOnFade))) {
            return;
        }
    }

    {
        std::lock_guard guard(analysisMutex);
        // Check again to avoid a race condition
        if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
        lastSplitTime = currentFrame.timestamp;
        splitIndex++;
    }

    if (haveToDetectTitleScreen) {
        callback.startTimer();
    } else {
        callback.split();
    }
}


static double maskNonZeroPart(const cv::UMat& mask);

bool isTitleScreen(const cv::UMat& gameRect) {
    // Both Sonic 1 and 2 have red backgrounds behind letters on the title screen.
    cv::UMat redColorMask;
    cv::inRange(gameRect, cv::Scalar(0, 0, 80), cv::Scalar(10, 10, 240), redColorMask);
    return maskNonZeroPart(redColorMask) > 0.015;
}


static cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold);

/**
 * currentFrame and previousFrame have to be the same size.
 * Detects the game rectangle on the screen if the game is currently fading to black
 * (on game start after title screen, after time bonus countdown, after death).
 * This is usually also the point to split in RTA-TB.
 * Returns an empty rect if a fade to black isn't going on or on detection failure.
 */
cv::Rect FrameAnalyzer::detectGameRectOnFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) const {
    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousFrame.frame, currentFrame.frame, 0.0);
    // The game can take a part of the screen. Also fading has a limited color pallete, so only a part of the image fades.
    if (maskNonZeroPart(decreasedBrightnessMask) < 0.15) return {};

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(decreasedBrightnessMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return {};
    const std::vector<cv::Point>& largestContour = std::ranges::max(contours, {}, [](const std::vector<cv::Point>& contour) {
        return cv::contourArea(contour);
    });
    cv::Rect gameRect = cv::boundingRect(largestContour);
    // Sometimes because of limited color pallete, the game may be split horizontally into several contours. Try to join them.
    for (const std::vector<cv::Point>& contour : contours) {
        if (&contour == &largestContour) continue;
        cv::Rect boundingRect = cv::boundingRect(contour);
        if (std::abs(((double) boundingRect.width - gameRect.width) / gameRect.width) < 0.05 &&
            std::abs(((double) boundingRect.x - gameRect.x) / gameRect.width) < 0.05) {
            gameRect |= boundingRect;
        }
    }

    if (gameRect.empty()) return {};
    double aspectRatio = ((double) gameRect.width) / gameRect.height;
    double expectedAspectRatio = settings.isStretchedTo16By9 ? (16. / 9) : (4. / 3);
    if (std::abs(aspectRatio - expectedAspectRatio) > 0.1) return {};
    if (gameRect.height < currentFrame.frame.rows / 2) return {};

    if (maskNonZeroPart(decreasedBrightnessMask(gameRect)) < 0.6) return {};
    cv::UMat gameRectBrightnessIncrease =
        calculateIncreasedBrightnessMask(currentFrame.frame(gameRect), previousFrame.frame(gameRect), 3.0);

    if (maskNonZeroPart(gameRectBrightnessIncrease) > 0.15) return {};
    return gameRect;
}


// mask should be 8-bit single channel, with values being either 0 or 255.
double maskNonZeroPart(const cv::UMat& mask) {
    return cv::mean(mask)[0] / 255.0;
}


// src1 and src2 must have three 8-bit channels.
// Returns a single-channel 8-bit mask of pixels that increased from src2 to src1 by more than the `threshold`.
cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold) {
    cv::UMat difference;
    cv::subtract(src1, src2, difference, cv::noArray(), CV_32S);
    std::vector<cv::UMat> channels;
    channels.reserve(3);
    cv::split(difference, channels);
    assert(channels.size() == 3);
    cv::add(channels[0], channels[1], channels[0]);
    cv::add(channels[0], channels[2], channels[0]);
    cv::UMat thresholdResult = channels[0];
    thresholdResult.convertTo(thresholdResult, CV_8U);
    cv::threshold(thresholdResult, thresholdResult, threshold / 3, 255.0, cv::THRESH_BINARY);
    return thresholdResult;
}


FrameAnalyzer::OnFrameCapturedListenerImpl::OnFrameCapturedListenerImpl(FrameAnalyzer& frameAnalyzer) : frameAnalyzer(frameAnalyzer) {}


void FrameAnalyzer::OnFrameCapturedListenerImpl::onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) {
    std::lock_guard guard(frameAnalyzer.frameMutex);
    swap(frameAnalyzer.currentFrame, frameAnalyzer.previousFrame);
    frameAnalyzer.currentFrame = capturedFrame;
    frameAnalyzer.analysisThreadPool.enqueue_detach([this, currentFrame = frameAnalyzer.currentFrame, previousFrame = frameAnalyzer.previousFrame]() {
        frameAnalyzer.analyzeFrame(currentFrame, previousFrame);
    });
}

}  // namespace RTA
}  // namespace SonicVisualSplitBase
