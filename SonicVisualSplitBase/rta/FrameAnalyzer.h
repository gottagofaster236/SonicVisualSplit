#pragma once
#include "../AnalysisSettings.h"
#include "../VideoCaptureManager.h"
#include "ThreadPool.h"
#include "AnalysisResult.h"
#include <opencv2/core.hpp>
#include <mutex>
#include <optional>

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
        virtual void onGameRectUpdated(const cv::Rect& gameRect) = 0;
    };

    FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback);

    ~FrameAnalyzer();

    AnalysisResult getLastAnalysisResult();

    void reportSplitIndex(int splitIndex);

private:
    void analyzeFrame(const VideoCaptureManager::CapturedFrame& currentFrame, const VideoCaptureManager::CapturedFrame& previousFrame);

    cv::Rect detectGameRectOnFade(const VideoCaptureManager::CapturedFrame& currentFrame, const VideoCaptureManager::CapturedFrame& previousFrame) const;

    bool detectGameRectOnSegaScreen(const VideoCaptureManager::CapturedFrame& frame, const cv::Rect& gameRectOnFade);

    bool verifyGameRectAspectRatio(const cv::Rect& gameRect) const;
    
    bool isSupportedGame() const;

    const AnalysisSettings settings;
    TimerCallback& callback;
    const cv::UMat resetTemplate;

    dp::thread_pool<> analysisThreadPool;

    VideoCaptureManager::CapturedFrame currentFrame;
    VideoCaptureManager::CapturedFrame previousFrame;
    std::mutex frameMutex;

    cv::Rect gameRect;
    cv::Size gameRectFrameSize;
    long long gameRectTimestamp;
    long long lastSplitTime;
    int splitIndex;
    long long lastResetCheckTime;
    mutable std::mutex analysisMutex;

    class OnFrameCapturedListenerImpl : public VideoCaptureManager::OnFrameCapturedListener {
    public:
        OnFrameCapturedListenerImpl(FrameAnalyzer& frameAnalyzer);

        void onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) final override;

        bool needsHighQualityResize() final override;
    private:
        FrameAnalyzer& frameAnalyzer;
    } onFrameCapturedListener;
};

}
}
