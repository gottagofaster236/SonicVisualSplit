#include "RTA.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/rta/FrameAnalyzer.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>

namespace SonicVisualSplitWrapper {
namespace RTA {

class TimerCallbackWrapper : public SonicVisualSplitBase::RTA::FrameAnalyzer::TimerCallback {
public:
    TimerCallbackWrapper(FrameAnalyzer::TimerCallback^ timerCallback) : timerCallback(timerCallback) {}

    virtual void startTimer() override {
        timerCallback->StartTimer();
    }

    virtual void split() override {
        timerCallback->Split();
    }

    virtual void reset() override {
        timerCallback->Reset();
    }

    virtual void pauseTimer() override {
        timerCallback->PauseTimer();
    }

    virtual void unpauseTimer() override {
        timerCallback->UnpauseTimer();
    }
private:
    gcroot<FrameAnalyzer::TimerCallback^> timerCallback;
};


FrameAnalyzer::FrameAnalyzer(AnalysisSettings^ settings, TimerCallback^ timerCallback) : settings(settings) {
    auto nativeTimerCallback = new TimerCallbackWrapper(timerCallback);
    nativeTimerCallbackPtr = System::IntPtr(nativeTimerCallback);
    auto nativeFrameAnalyzer = new SonicVisualSplitBase::RTA::FrameAnalyzer(
        static_cast<SonicVisualSplitBase::AnalysisSettings>(settings), *nativeTimerCallback);
    nativeFrameAnalyzerPtr = System::IntPtr(nativeFrameAnalyzer);
}


FrameAnalyzer::~FrameAnalyzer() {
    delete static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    delete static_cast<TimerCallbackWrapper*>(nativeTimerCallbackPtr.ToPointer());
}


System::Drawing::Bitmap^ FrameAnalyzer::VisualizeLastFrame() {
    auto nativeFrameAnalyzer = static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    return toBitmap(nativeFrameAnalyzer->visualizeLastFrame());
}

void FrameAnalyzer::OnReset() {
    auto nativeFrameAnalyzer = static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    nativeFrameAnalyzer->onReset();
}


AnalysisSettings^ FrameAnalyzer::Settings::get() {
    return settings;
}

}  // namespace RTA
}  // namespace SonicVisualSplitWrapper 
