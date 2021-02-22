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
};

public ref class FrameAnalyzer {
public:
    FrameAnalyzer(String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9, Boolean isComposite);

    AnalysisResult^ AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean recalculateOnError, Boolean visualize);

private:
    String^ gameName;
    String^ templatesDirectory;
    Boolean isStretchedTo16By9;
    Boolean isComposite;
};

public ref class BaseWrapper {
public:
    static void StartSavingFrames();

    static void StopSavingFrames();

    static List<Int64>^ GetSavedFramesTimes();

    static void DeleteSavedFrame(Int64 frameTime);

    static void DeleteSavedFramesBefore(Int64 frameTime);

    static void DeleteAllSavedFrames();

    static void DeleteSavedFramesInRange(Int64 beginFrameTime, Int64 endFrameTime);
};

}  // namespace SonicVisualSplitWrapper