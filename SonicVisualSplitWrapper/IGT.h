#pragma once

#include "Common.h"

namespace SonicVisualSplitWrapper {
namespace IGT {

public enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

public ref class AnalysisResult {
public:
    property System::Boolean RecognizedTime;
    property System::Int32 TimeInMilliseconds;
    property System::String^ TimeString;
    property System::Boolean IsScoreScreen;
    property System::Boolean IsBlackScreen;
    property System::Boolean IsWhiteScreen;
    property System::Drawing::Bitmap^ VisualizedFrame;
    property ErrorReasonEnum ErrorReason;
    property System::Int64 FrameTime;

    System::Boolean IsSuccessful();
    void MarkAsIncorrectlyRecognized();
    virtual System::String^ ToString() override;  // For debugging.
};


public ref class FrameStorage {
public:
    System::Collections::Generic::List<System::Int64>^ GetSavedFramesTimes();

    void DeleteSavedFrame(System::Int64 frameTime);

    void DeleteSavedFramesBefore(System::Int64 frameTime);

    void DeleteAllSavedFrames();

    void DeleteSavedFramesInRange(System::Int64 beginFrameTime, System::Int64 endFrameTime);

    int GetMaxCapacity();

internal:
    FrameStorage(System::IntPtr nativeFrameStoragePtr);

private:
    System::IntPtr nativeFrameStoragePtr;
};


public ref class FrameAnalyzer {
public:
    /* Creates a new instance of FrameAnalyzer if the parameters differ from the oldInstance.
     * Calls Dispose() for the oldInstance if the new instance  */
    static void createNewInstanceIfNeeded(FrameAnalyzer^% oldInstance,
        AnalysisSettings^ settings);

    ~FrameAnalyzer();

    AnalysisResult^ AnalyzeFrame(System::Int64 frameTime, System::Boolean checkForScoreScreen, System::Boolean visualize);

    System::Boolean CheckForResetScreen();

    void ReportCurrentSplitIndex(int currentSplitIndex);

    void ResetDigitsLocation();

    property FrameStorage^ FrameStorage {
        IGT::FrameStorage^ get();
    }

private:
    FrameAnalyzer(AnalysisSettings^ settings);

    System::IntPtr nativeFrameAnalyzerPtr;
    AnalysisSettings^ settings;
    IGT::FrameStorage^ _frameStorage;
};


}
}