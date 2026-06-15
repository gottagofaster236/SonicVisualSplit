#include "RTA.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/rta/FrameAnalyzer.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>

namespace SonicVisualSplitWrapper {
namespace RTA {

class TimerCallbackWrapper : public SonicVisualSplitBase::RTA::FrameAnalyzer::TimerCallback {
public:
    TimerCallbackWrapper(FrameAnalyzer^ frameAnalyzer, FrameAnalyzer::TimerCallback^ timerCallback) : 
        frameAnalyzer(frameAnalyzer), timerCallback(timerCallback) {}

    virtual void startTimer() final override {
        timerCallback->StartTimer();
    }

    virtual void split() final override {
        timerCallback->Split();
    }

    virtual void reset() final override {
        timerCallback->Reset();
    }

    virtual void pauseTimer() final override {
        timerCallback->PauseTimer();
    }

    virtual void unpauseTimer() final override {
        timerCallback->UnpauseTimer();
    }

    virtual void onGameRectUpdated(const cv::Rect& gameRect) final override {
        frameAnalyzer->GameRect = System::Nullable<System::Drawing::Rectangle>(System::Drawing::Rectangle(gameRect.x, gameRect.y, gameRect.width, gameRect.height));
    }
private:
    gcroot<FrameAnalyzer^> frameAnalyzer;
    gcroot<FrameAnalyzer::TimerCallback^> timerCallback;
};


FrameAnalyzer::FrameAnalyzer(AnalysisSettings^ settings, TimerCallback^ timerCallback) : settings(settings) {
    auto nativeTimerCallback = new TimerCallbackWrapper(this, timerCallback);
    nativeTimerCallbackPtr = System::IntPtr(nativeTimerCallback);
    auto nativeFrameAnalyzer = new SonicVisualSplitBase::RTA::FrameAnalyzer(
        static_cast<SonicVisualSplitBase::AnalysisSettings>(settings), *nativeTimerCallback);
    nativeFrameAnalyzerPtr = System::IntPtr(nativeFrameAnalyzer);
}


FrameAnalyzer::~FrameAnalyzer() {
    delete static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    delete static_cast<TimerCallbackWrapper*>(nativeTimerCallbackPtr.ToPointer());
}


AnalysisResult^ FrameAnalyzer::GetLastAnalysisResult() {
    auto nativeFrameAnalyzer = static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    SonicVisualSplitBase::RTA::AnalysisResult result = nativeFrameAnalyzer->getLastAnalysisResult();
    AnalysisResult^ resultConverted = gcnew AnalysisResult();
    resultConverted->ErrorReason = static_cast<ErrorReasonEnum>(result.errorReason);
    resultConverted->VisualizedFrame = toBitmap(result.visualizedFrame);
    return resultConverted;
}

void FrameAnalyzer::ReportSplitIndex(int splitIndex) {
    auto nativeFrameAnalyzer = static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    nativeFrameAnalyzer->reportSplitIndex(splitIndex);
}


AnalysisSettings^ FrameAnalyzer::Settings::get() {
    return settings;
}

}  // namespace RTA
}  // namespace SonicVisualSplitWrapper 
