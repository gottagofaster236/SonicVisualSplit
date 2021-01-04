#pragma once

namespace SonicVisualSplitWrapper {

using namespace System;
using System::Collections::Generic::List;
using System::Drawing::Rectangle;
using System::Drawing::Bitmap;

public enum ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

public ref class AnalysisResult {
public:
    Boolean FoundAnyDigits;
    String^ TimeDigits;
    Boolean IsScoreScreen;
    Boolean IsBlackScreen;
    Bitmap^ VisualizedFrame;
    ErrorReasonEnum ErrorReason;
};

public ref class FrameAnalyzer {
public:
    FrameAnalyzer(String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9);

    AnalysisResult^ AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize, Boolean recalculateOnError);

private:
    String^ gameName;
    String^ templatesDirectory;
    Boolean isStretchedTo16By9;
};

public ref class BaseWrapper {
public:
    static void StartSavingFrames();

    static List<Int64>^ GetSavedFramesTimes();

    static void DeleteSavedFramesBefore(Int64 frameTime);
};

}  // namespace SonicVisualSplitWrapper