#pragma once
#include "../AnalysisSettings.h"
#include "../VideoCaptureManager.h"
#include "ThreadPool.h"
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {
namespace RTA {

class FrameAnalyzer {
public:
    class TimerCallback {
    public:
        virtual void startTimer() = 0;
        virtual void split() = 0;
        virtual void reset() = 0;
        virtual void pauseTimer() = 0;
        virtual void unpauseTimer() = 0;
    };

    FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback);

    ~FrameAnalyzer();

    cv::Mat visualizeLastFrame();

    void onReset();

private:
    void analyzeFrame(const VideoCaptureManager::CapturedFrame& currentFrame, const VideoCaptureManager::CapturedFrame& previousFrame);

    cv::Rect detectGameRectOnFade(const VideoCaptureManager::CapturedFrame& currentFrame, const VideoCaptureManager::CapturedFrame& previousFrame) const;

    const AnalysisSettings settings;
    TimerCallback& callback;

    dp::thread_pool<> analysisThreadPool;

    VideoCaptureManager::CapturedFrame currentFrame;
    VideoCaptureManager::CapturedFrame previousFrame;
    std::mutex frameMutex;

    cv::Rect gameRect;
    long long gameRectTimestamp;
    long long lastSplitTime;
    int splitIndex;
    std::mutex analysisMutex;

    class OnFrameCapturedListenerImpl : public VideoCaptureManager::OnFrameCapturedListener {
    public:
        OnFrameCapturedListenerImpl(FrameAnalyzer& frameAnalyzer);

        void onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) override;
    private:
        FrameAnalyzer& frameAnalyzer;
    } onFrameCapturedListener;
};

}
}
