﻿#pragma once

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
    Boolean FoundAnyDigits;
    String^ TimeDigits;
    Boolean IsScoreScreen;
    Boolean IsBlackScreen;
    Bitmap^ VisualizedFrame;
    ErrorReasonEnum ErrorReason;

    Boolean IsSuccessful();
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

    static void StopSavingFrames();

    static List<Int64>^ GetSavedFramesTimes();

    static void DeleteSavedFramesBefore(Int64 frameTime);

    static void DeleteAllSavedFrames();
};

}  // namespace SonicVisualSplitWrapper