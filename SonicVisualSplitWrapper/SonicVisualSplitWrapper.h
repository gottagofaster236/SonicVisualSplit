#pragma once


// Forward-declaration.
namespace SonicVisualSplitBase {
    class FrameAnalyzer;
}


namespace SonicVisualSplitWrapper {

using namespace System;
using System::Collections::Generic::List;
using System::Drawing::Rectangle;
using System::Drawing::Bitmap;

public enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

public ref class AnalysisResult {
public:
    Boolean RecognizedTime;
    Int32 TimeInMilliseconds;
    String^ TimeString;
    Boolean IsScoreScreen;
    Boolean IsBlackScreen;
    Boolean IsWhiteScreen;
    Bitmap^ VisualizedFrame;
    ErrorReasonEnum ErrorReason;
    Int64 FrameTime;

    Boolean IsSuccessful();
    void MarkAsIncorrectlyRecognized();
    virtual String^ ToString() override;  // For debugging.
};


public ref class FrameAnalyzer {
public:
    /* Creates a new instance of FrameAnalyzer if the parameters differ from the oldInstance.
     * Calls Dispose() for the oldInstance if the new instance  */ 
    static void createNewInstanceIfNeeded(FrameAnalyzer^% oldInstance,
        String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9, Boolean isComposite);

    ~FrameAnalyzer();

    AnalysisResult^ AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize);

    void ReportCurrentSplitIndex(int currentSplitIndex);

    void ResetDigitsPlacement();

private:
    FrameAnalyzer(String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9, Boolean isComposite);

    String^ gameName;
    String^ templatesDirectory;
    Boolean isStretchedTo16By9;
    Boolean isComposite;

    IntPtr nativeFrameAnalyzerPtr;
};


public ref class FrameStorage {
public:
    static void SetVideoCapture(int sourceIndex);

    static void StartSavingFrames();

    static void StopSavingFrames();

    static List<Int64>^ GetSavedFramesTimes();

    static void DeleteSavedFrame(Int64 frameTime);

    static void DeleteSavedFramesBefore(Int64 frameTime);

    static void DeleteAllSavedFrames();

    static void DeleteSavedFramesInRange(Int64 beginFrameTime, Int64 endFrameTime);

    static int GetMaxCapacity();

    static Int64 GetCurrentTimeInMilliseconds();

    literal int NO_VIDEO_CAPTURE = -1, OBS_WINDOW_CAPTURE = -2;
};


public ref class VirtualCamCapture {
public:
    static List<String^>^ GetVideoDevicesList();
};

}  // namespace SonicVisualSplitWrapper