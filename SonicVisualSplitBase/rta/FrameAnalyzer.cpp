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
    settings(settings), callback(callback), onFrameCapturedListener(*this), lastSplitTime(0) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}

FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
}


cv::Mat FrameAnalyzer::visualizeLastFrame() {
    return cv::Mat();  // TODO
}


static cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold);

void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }

    {
        std::lock_guard guard(analysisMutex);
        if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
    }

    const cv::UMat frame = currentFrame.frame;

    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousFrame.frame, currentFrame.frame, 0.0);
    // The game can take a part of the screen, plus, fading in Sonic has limited colors, so only a part of the image fades.
    if (cv::mean(decreasedBrightnessMask)[0] < 0.15) return;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(decreasedBrightnessMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return;
    const std::vector<cv::Point>& largestContour = std::ranges::max(contours, {}, [](const std::vector<cv::Point>& contour) {
        return cv::contourArea(contour);
    });
    cv::Rect gameRect = cv::boundingRect(largestContour);
    // Sometimes because of limited colors, the game may be split horizontally into several contours. Try to join them.
    for (const std::vector<cv::Point>& contour : contours) {
        if (&contour == &largestContour) continue;
        cv::Rect boundingRect = cv::boundingRect(contour);
        if (std::abs(((double) boundingRect.width - gameRect.width) / gameRect.width) < 0.05 &&
            std::abs(((double) boundingRect.x - gameRect.x) / gameRect.width) < 0.05) {
            gameRect |= boundingRect;
        }
    }

    if (gameRect.empty()) return;    
    double aspectRatio = ((double) gameRect.width) / gameRect.height;
    double expectedAspectRatio = settings.isStretchedTo16By9 ? (16. / 9) : (4. / 3);
    if (std::abs(aspectRatio - expectedAspectRatio) > 0.1) return;
    if (gameRect.height < frame.rows / 2) return;
    
    if (cv::mean(decreasedBrightnessMask(gameRect))[0] < 0.4) return;
    cv::UMat gameRectBrightnessIncrease = 
        calculateIncreasedBrightnessMask(currentFrame.frame(gameRect), previousFrame.frame(gameRect), 10.0);

    if (cv::mean(gameRectBrightnessIncrease)[0] > 0.05) return;

    cv::UMat visualizedFrame;
    frame.copyTo(visualizedFrame);
    int lineThickness = 1 + frame.rows / 500;
    cv::rectangle(visualizedFrame, gameRect, cv::Scalar(0, 0, 255), lineThickness);
    cv::drawContours(visualizedFrame, contours, -1, cv::Scalar(255, 0, 0), lineThickness);
    cv::imwrite("C:\\tmp\\" + std::to_string(currentFrame.timestamp) + "contours.png", visualizedFrame);

    {
        std::lock_guard guard(analysisMutex);
        if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
        lastSplitTime = currentFrame.timestamp;
    }
    callback.split();
}


// src1 and src2 must have three 8-bit channels
cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold) {
    cv::UMat converted;
    src1.convertTo(converted, CV_32S);
    cv::UMat difference;
    cv::subtract(src1, src2, difference);
    std::vector<cv::UMat> channels;
    channels.reserve(3);
    cv::split(difference, channels);
    assert(channels.size() == 3);
    cv::add(channels[0], channels[1], channels[0]);
    cv::add(channels[0], channels[2], channels[0]);
    cv::UMat thresholdResult = channels[0];
    thresholdResult.convertTo(thresholdResult, CV_8U);
    cv::threshold(thresholdResult, thresholdResult, threshold, 1.0, cv::THRESH_BINARY);
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
