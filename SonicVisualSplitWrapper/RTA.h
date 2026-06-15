#pragma once
#include "Common.h"

namespace SonicVisualSplitWrapper {
namespace RTA {

public enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_GAME_RECT, UNSUPPORTED_GAME, NO_ERROR,
};

public ref class AnalysisResult {
public:
    property System::Drawing::Bitmap^ VisualizedFrame;
    property ErrorReasonEnum ErrorReason;
};

class TimerCallbackWrapper;

public ref class FrameAnalyzer {
public:
    interface class TimerCallback {
        void StartTimer();
        void Split();
        void Reset();
        void PauseTimer();
        void UnpauseTimer();
    };

    FrameAnalyzer(AnalysisSettings^ settings, TimerCallback^ timerCallback);

    ~FrameAnalyzer();

    AnalysisResult^ GetLastAnalysisResult();

    void ReportSplitIndex(int splitIndex);

    property AnalysisSettings^ Settings {
        AnalysisSettings^ get();
    }

    volatile System::Nullable<System::Drawing::Rectangle> GameRect;

private:
    AnalysisSettings^ settings;
    System::IntPtr nativeTimerCallbackPtr;
    System::IntPtr nativeFrameAnalyzerPtr;
};

}  // namespace RTA
}  // namespace SonicVisualSplitWrapper 
