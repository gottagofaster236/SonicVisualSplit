#include "IGT.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/igt/FrameAnalyzer.h"
#include "../SonicVisualSplitBase/igt/FrameStorage.h"
#pragma managed(pop)
#include <algorithm>

using namespace System;
using System::Collections::Generic::List;

namespace SonicVisualSplitWrapper {
namespace IGT {

SonicVisualSplitBase::IGT::FrameAnalyzer* getFrameAnalyzerFromIntPtr(System::IntPtr ptr) {
    return static_cast<SonicVisualSplitBase::IGT::FrameAnalyzer*>(ptr.ToPointer());
}


SonicVisualSplitBase::IGT::FrameStorage* getFrameStorageFromIntPtr(System::IntPtr ptr) {
    return static_cast<SonicVisualSplitBase::IGT::FrameStorage*>(ptr.ToPointer());
}


FrameAnalyzer::FrameAnalyzer(AnalysisSettings^ settings) : settings(settings) {
    auto nativeFrameAnalyzer = new SonicVisualSplitBase::IGT::FrameAnalyzer(static_cast<SonicVisualSplitBase::AnalysisSettings>(settings));
    nativeFrameAnalyzerPtr = System::IntPtr(nativeFrameAnalyzer);
    frameStorage = gcnew IGT::FrameStorage(System::IntPtr(&nativeFrameAnalyzer->frameStorage));
}


FrameAnalyzer::~FrameAnalyzer() {
    delete getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr);
}


// Converting non-managed types to managed ones to call the native version of the function
AnalysisResult^ FrameAnalyzer::AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize, System::Nullable<System::Drawing::Rectangle> gameRect) {
    auto nativeFrameAnalyzer = getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr);
    std::optional<cv::Rect> gameRectOptional;
    if (gameRect.HasValue) {
        gameRectOptional = {{gameRect.Value.Left, gameRect.Value.Top, gameRect.Value.Width, gameRect.Value.Height}};
    }

    SonicVisualSplitBase::IGT::AnalysisResult result =
        nativeFrameAnalyzer->analyzeFrame(frameTime, checkForScoreScreen, visualize, gameRectOptional);

    AnalysisResult^ resultConverted = gcnew AnalysisResult();
    resultConverted->RecognizedTime = result.recognizedTime;
    resultConverted->TimeInMilliseconds = result.timeInMilliseconds;
    resultConverted->TimeString = gcnew String(result.timeString.c_str());
    resultConverted->IsScoreScreen = result.isScoreScreen;
    resultConverted->IsBlackScreen = result.isBlackScreen;
    resultConverted->IsWhiteScreen = result.isWhiteScreen;
    resultConverted->ErrorReason = static_cast<ErrorReasonEnum>(result.errorReason);
    resultConverted->FrameTime = result.frameTime;

    if (resultConverted->ErrorReason != ErrorReasonEnum::VIDEO_DISCONNECTED && visualize) {
        resultConverted->VisualizedFrame = toBitmap(result.visualizedFrame);
    } else {
        resultConverted->VisualizedFrame = nullptr;
    }
    return resultConverted;
}


Boolean FrameAnalyzer::CheckForResetScreen() {
    return getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr)->checkForResetScreen();
}


void FrameAnalyzer::ReportCurrentSplitIndex(int currentSplitIndex) {
    getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr)->reportCurrentSplitIndex(currentSplitIndex);
}


void FrameAnalyzer::ResetDigitsLocation() {
    getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr)->resetDigitsLocation();
}


IGT::FrameStorage^ FrameAnalyzer::FrameStorage::get() {
    return frameStorage;
}


AnalysisSettings^ FrameAnalyzer::Settings::get() {
    return settings;
}


Boolean AnalysisResult::IsSuccessful() {
    return ErrorReason == ErrorReasonEnum::NO_ERROR;
}


void AnalysisResult::MarkAsIncorrectlyRecognized() {
    if (IsSuccessful())
        ErrorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
}


String^ AnalysisResult::ToString() {
    return String::Format("Frame time: {0}, recognized time: {1}, isBlack: {2}, isWhite: {3}, isScoreScreen: {4}",
        FrameTime, TimeString, IsBlackScreen, IsWhiteScreen, IsScoreScreen);
}


List<Int64>^ FrameStorage::GetSavedFramesTimes() {
    std::vector<long long> savedFramesTimes = getFrameStorageFromIntPtr(nativeFrameStoragePtr)->getSavedFramesTimes();
    List<Int64>^ converted = gcnew List<Int64>((int) savedFramesTimes.size());
    for (long long frameTime : savedFramesTimes) {
        converted->Add(frameTime);
    }
    return converted;
}


void FrameStorage::DeleteSavedFrame(Int64 frameTime) {
    DeleteSavedFramesInRange(frameTime, frameTime + 1);
}


void FrameStorage::DeleteSavedFramesBefore(Int64 frameTime) {
    DeleteSavedFramesInRange(INT64_MIN, frameTime);
}


void FrameStorage::DeleteAllSavedFrames() {
    DeleteSavedFramesInRange(INT64_MIN, INT64_MAX);
}


void FrameStorage::DeleteSavedFramesInRange(Int64 beginFrameTime, Int64 endFrameTime) {
    getFrameStorageFromIntPtr(nativeFrameStoragePtr)->deleteSavedFramesInRange(beginFrameTime, endFrameTime);
}


int FrameStorage::GetMaxCapacity() {
    return SonicVisualSplitBase::IGT::FrameStorage::MAX_CAPACITY;
}


FrameStorage::FrameStorage(System::IntPtr nativeFrameStoragePtr) : nativeFrameStoragePtr(nativeFrameStoragePtr) {}

}  // namespace IGT
}  // namespace SonicVisualSplitWrapper
