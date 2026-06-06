#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace SonicVisualSplitBase {
namespace RTA {

using VideoCaptureManager::CapturedFrame;

FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback) :
    settings(settings), callback(callback), onFrameCapturedListener(*this) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}

FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
}


cv::Mat FrameAnalyzer::visualizeLastFrame() {
    return cv::Mat();  // TODO
}


void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }
    cv::UMat difference;
    cv::subtract(previousFrame.frame, currentFrame.frame, difference);
    cv::cvtColor(difference, difference, cv::COLOR_BGR2GRAY);
    cv::threshold(difference, difference, 5.0, 255.0, cv::THRESH_BINARY);
    double minVal, maxVal;
    cv::minMaxLoc(difference, &minVal, &maxVal);
    if (maxVal < 5.0) return;
    cv::imwrite("C:\\tmp\\difference.png", difference);
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