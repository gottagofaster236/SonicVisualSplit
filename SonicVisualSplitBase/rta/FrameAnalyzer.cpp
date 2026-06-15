#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace SonicVisualSplitBase {
namespace RTA {

// Avoid several consecutive splits at once
constexpr long long WAIT_AFTER_SPLIT_MS = 3000;

using VideoCaptureManager::CapturedFrame;

FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback) :
    settings(settings), callback(callback),
    gameRect{}, gameRectTimestamp(0), lastSplitTime(0), splitIndex(0), lastResetCheckTime(0),
    onFrameCapturedListener(*this) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}


FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
}


AnalysisResult FrameAnalyzer::getLastAnalysisResult() {
    AnalysisResult result;

    CapturedFrame currentFrame;
    {
        std::lock_guard guard(frameMutex);
        currentFrame = this->currentFrame;
    }
    if (currentFrame.frame.empty() || VideoCaptureManager::getCurrentTimeInMilliseconds() - currentFrame.timestamp > 5000) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
    }

    cv::Rect gameRect;
    {
        std::lock_guard guard(analysisMutex);
        gameRect = this->gameRect;
    }

    currentFrame.frame.copyTo(result.visualizedFrame);
    if (!gameRect.empty()) {
        int lineThickness = 1 + result.visualizedFrame.rows / 500;
        cv::rectangle(result.visualizedFrame, gameRect, cv::Scalar(0, 0, 255), lineThickness);
    } else if (result.errorReason == ErrorReasonEnum::NO_ERROR) {
        result.errorReason = ErrorReasonEnum::NO_GAME_RECT;
    }

    if (!isSupportedGame()) {
        result.errorReason = ErrorReasonEnum::UNSUPPORTED_GAME;
    }

    return result;
}


bool FrameAnalyzer::isSupportedGame() const {
    return settings.game == Game::Sonic1 || settings.game == Game::Sonic2;
}


void FrameAnalyzer::onReset() {
    std::lock_guard guard(analysisMutex);
    splitIndex = 0;
}


void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (!isSupportedGame()) return;
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }

    cv::Rect gameRectOnFade = detectGameRectOnFade(currentFrame, previousFrame);
    if (gameRectOnFade.empty()) return;
    bool timerStarted;
    {
        std::lock_guard guard(analysisMutex);
        // Prefer the first game rectangle, as it operates with higher brightness and thus more accurate
        if (currentFrame.timestamp - gameRectTimestamp >= WAIT_AFTER_SPLIT_MS) {
            gameRect = gameRectOnFade;
            gameRectTimestamp = currentFrame.timestamp;
        }
        timerStarted = splitIndex != 0;  // Don't start timer on "SEGA" logo fadeout, that's too early
    }

    cv::UMat gameScreen = previousFrame.frame(gameRectOnFade);
    if (!timerStarted && !isTitleScreen(gameScreen)) {
        return;
    }

    if (timerStarted && isSegaScreen(gameScreen)) {
        {
            std::lock_guard guard(analysisMutex);
            if (lastSplitTime == 0) return;  // Check again to avoid a race condition
            lastSplitTime = 0;
        }
        callback.reset();
        return;
    }

    {
        std::lock_guard guard(analysisMutex);
        if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
        lastSplitTime = currentFrame.timestamp;
        splitIndex++;
    }

    if (!timerStarted) {
        callback.startTimer();
    } else {
        callback.split();
    }
}

static cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold);
static double maskNonZeroPart(const cv::UMat& mask);

/**
 * currentFrame and previousFrame have to be the same size.
 * Detects the game rectangle on the screen if the game is currently fading to black
 * (on game start after title screen, after time bonus countdown, after death).
 * This is usually also the point to split in RTA-TB.
 * Returns an empty rect if a fade to black isn't going on or on detection failure.
 */
cv::Rect FrameAnalyzer::detectGameRectOnFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) const {
    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousFrame.frame, currentFrame.frame, 3.0);
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


bool FrameAnalyzer::isTitleScreen(const cv::UMat& gameRect) {
    // Both Sonic 1 and 2 have red backgrounds behind letters on the title screen.
    cv::UMat redColorMask;
    cv::inRange(gameRect, cv::Scalar(0, 0, 80), cv::Scalar(30, 30, 240), redColorMask);
    return maskNonZeroPart(redColorMask) > 0.015;
}


bool FrameAnalyzer::isSegaScreen(const cv::UMat& gameRect) const {
    cv::UMat whiteColorMask;
    cv::inRange(gameRect, cv::Scalar(180, 180, 180), cv::Scalar(255, 255, 255), whiteColorMask);
    if (maskNonZeroPart(whiteColorMask) < 0.6) return false;
    cv::UMat blueColorMask;
    switch (settings.game) {
    case Game::Sonic1:
        cv::inRange(gameRect, cv::Scalar(180, 0, 0), cv::Scalar(255, 30, 30), blueColorMask);
        break;
    default:
        cv::inRange(gameRect, cv::Scalar(170, 60, 0), cv::Scalar(255, 130, 30), blueColorMask);
        break;
    }
    return maskNonZeroPart(blueColorMask) > 0.04;
}


static cv::UMat tryCrop(const cv::UMat& src, const cv::Rect& roi) {
    if (roi.x + roi.width > src.cols || roi.y + roi.height > src.rows) return {};
    return src(roi);
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


bool FrameAnalyzer::OnFrameCapturedListenerImpl::needsHighQualityResize() {
    return false;
}

}  // namespace RTA
}  // namespace SonicVisualSplitBase
