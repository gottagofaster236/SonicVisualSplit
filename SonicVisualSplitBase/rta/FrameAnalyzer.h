#pragma once
#include "../AnalysisSettings.h"
#include "../VideoCaptureManager.h"
#include "../TemplateMatcher.h"
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

    bool splitWithoutTimeBonus(int splitIndex) const;

    void processTimeBonus(const cv::UMat& gameRect, long long timestamp);
    bool getTimeBonusPoints(const cv::UMat& gameRect);
    cv::UMat cropToDigitsRect(const cv::UMat& gameRect) const;
    bool hasTimeBonus(const cv::UMat& gameRect) const;
    void pauseForTimeBonus(int timeBonusPoints);

    const AnalysisSettings settings;
    TimerCallback& callback;
    const TemplateMatcher templateMatcher;

    dp::thread_pool<> analysisThreadPool;

    // Section guarded by frameMutex
    VideoCaptureManager::CapturedFrame currentFrame;
    VideoCaptureManager::CapturedFrame previousFrame;
    std::mutex frameMutex;

    // Section guarded by analysisMutex
    cv::Rect gameRect;
    cv::Size gameRectFrameSize;
    long long gameRectTimestamp = 0;

    long long lastSplitTime = 0;
    int splitIndex = -1;

    enum class TimeBonusState {
        INITIAL, CONFIRM_TIME_BONUS_LABEL, CONFIRM_TIME_BONUS_POINTS, WAIT_FOR_COUNTDOWN_TO_START, PAUSED_FOR_TIME_BONUS, AFTER_TIME_BONUS,
    };
    TimeBonusState timeBonusState = TimeBonusState::INITIAL;
    long long lastTimeBonusCheckTime = 0;
    int timeBonusPoints = 0;
    std::string timeBonusString;
    TemplateMatcher::Match digitMatchToObserve;

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
