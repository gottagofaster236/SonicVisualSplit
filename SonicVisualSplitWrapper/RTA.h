#pragma once
#include "Common.h"

namespace SonicVisualSplitWrapper {
namespace RTA {

public ref class AnalysisResult {
public:
    long long FrameTime;
    int TimeBonusPoints;
};

class CallbackWrapper;

public ref class FrameAnalyzer {
public:
    interface class Callback {
        void StartTimer();
        void Split();
        void UndoSplit();
        void Reset();
        void PauseTimer();
        void UnpauseTimer();
        void OnAnalysisResult(AnalysisResult^ result);
    };

    FrameAnalyzer(AnalysisSettings^ settings, Callback^ callback);

    ~FrameAnalyzer();

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
