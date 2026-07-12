#include "RTA.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/rta/FrameAnalyzer.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>

namespace SonicVisualSplitWrapper {
namespace RTA {

using namespace System;

class CallbackWrapper : public SonicVisualSplitBase::RTA::FrameAnalyzer::Callback {
public:
    CallbackWrapper(FrameAnalyzer^ frameAnalyzer, FrameAnalyzer::Callback^ callback) : 
        frameAnalyzer(frameAnalyzer), callback(callback) {}

    virtual void startTimer() final override {
        callback->StartTimer();
    }

    virtual void split() final override {
        callback->Split();
    }

    virtual void undoSplit() final override {
        callback->UndoSplit();
    }

    virtual void reset() final override {
        callback->Reset();
    }

    virtual void pauseTimer() final override {
        callback->PauseTimer();
    }

    virtual void unpauseTimer() final override {
        callback->UnpauseTimer();
    }

    virtual void onGameRectUpdated(const cv::Rect& gameRect) final override {
        frameAnalyzer->GameRect = Nullable<Drawing::Rectangle>(Drawing::Rectangle(gameRect.x, gameRect.y, gameRect.width, gameRect.height));
    }
private:
    gcroot<FrameAnalyzer^> frameAnalyzer;
    gcroot<FrameAnalyzer::Callback^> callback;
};


FrameAnalyzer::FrameAnalyzer(AnalysisSettings^ settings, Callback^ callback) : settings(settings) {
    auto nativeTimerCallback = new CallbackWrapper(this, callback);
    nativeTimerCallbackPtr = IntPtr(nativeTimerCallback);
    auto nativeFrameAnalyzer = new SonicVisualSplitBase::RTA::FrameAnalyzer(
        static_cast<SonicVisualSplitBase::AnalysisSettings>(settings), *nativeTimerCallback);
    nativeFrameAnalyzerPtr = IntPtr(nativeFrameAnalyzer);
}


FrameAnalyzer::~FrameAnalyzer() {
    delete static_cast<SonicVisualSplitBase::RTA::FrameAnalyzer*>(nativeFrameAnalyzerPtr.ToPointer());
    delete static_cast<CallbackWrapper*>(nativeTimerCallbackPtr.ToPointer());
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
