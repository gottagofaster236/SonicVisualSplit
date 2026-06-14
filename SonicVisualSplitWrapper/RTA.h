#pragma once
#include "Common.h"

namespace SonicVisualSplitWrapper {
namespace RTA {

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

    System::Drawing::Bitmap^ VisualizeLastFrame();

    void OnReset();

    property AnalysisSettings^ Settings {
        AnalysisSettings^ get();
    }

private:
    AnalysisSettings^ settings;
    System::IntPtr nativeTimerCallbackPtr;
    System::IntPtr nativeFrameAnalyzerPtr;
};

}  // namespace RTA
}  // namespace SonicVisualSplitWrapper 
