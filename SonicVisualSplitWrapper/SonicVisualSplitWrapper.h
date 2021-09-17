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

public ref class AnalysisSettings {
public:
    AnalysisSettings(String^ gameName, String^ templatesDirectory,
        Boolean isStretchedTo16By9, Boolean isComposite);

    Boolean Equals(Object^ other) override;

    property String^ GameName;
    property String^ TemplatesDirectory;
    property Boolean IsStretchedTo16By9;
    property Boolean IsComposite;
};

public enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

public ref class AnalysisResult {
public:
    property Boolean RecognizedTime;
    property Int32 TimeInMilliseconds;
    property String^ TimeString;
    property Boolean IsScoreScreen;
    property Boolean IsBlackScreen;
    property Boolean IsWhiteScreen;
    property Bitmap^ VisualizedFrame;
    property ErrorReasonEnum ErrorReason;
    property Int64 FrameTime;

    Boolean IsSuccessful();
    void MarkAsIncorrectlyRecognized();
    virtual String^ ToString() override;  // For debugging.
};


public ref class FrameAnalyzer {
public:
    /* Creates a new instance of FrameAnalyzer if the parameters differ from the oldInstance.
     * Calls Dispose() for the oldInstance if the new instance  */ 
    static void createNewInstanceIfNeeded(FrameAnalyzer^% oldInstance,
        AnalysisSettings^ settings);

    ~FrameAnalyzer();

    AnalysisResult^ AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize);

    Boolean CheckForResetScreen(Int64 frameTime);

    void ReportCurrentSplitIndex(int currentSplitIndex);

    void ResetDigitsPositions();

private:
    FrameAnalyzer(AnalysisSettings^ settings);

    AnalysisSettings^ settings;
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