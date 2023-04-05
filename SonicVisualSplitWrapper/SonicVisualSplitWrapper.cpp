#include "SonicVisualSplitWrapper.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/FrameAnalyzer.h"
#include "../SonicVisualSplitBase/FrameStorage.h"
#include "../SonicVisualSplitBase/VirtualCamCapture.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>
#include <algorithm>
#undef NO_ERROR  // defined in WinError.h
using System::Drawing::Imaging::PixelFormat;
using System::Drawing::Imaging::ImageLockMode;
using System::Drawing::Imaging::BitmapData;

namespace SonicVisualSplitWrapper {

AnalysisSettings::AnalysisSettings(String^ gameName, String^ templatesDirectory,
        Boolean isStretchedTo16By9, Boolean isComposite) {
    GameName = gameName;
    TemplatesDirectory = templatesDirectory;
    IsStretchedTo16By9 = isStretchedTo16By9;
    IsComposite = isComposite;
}


Boolean AnalysisSettings::Equals(Object^ other) {
    AnalysisSettings^ otherSettings = dynamic_cast<AnalysisSettings^>(other);
    return otherSettings &&
        otherSettings->GameName == GameName &&
        otherSettings->TemplatesDirectory == TemplatesDirectory &&
        otherSettings->IsStretchedTo16By9 == IsStretchedTo16By9 &&
        otherSettings->IsComposite == IsComposite;
}


void FrameAnalyzer::createNewInstanceIfNeeded(FrameAnalyzer^% oldInstance, AnalysisSettings^ settings) {
    bool shouldCreateNewInstance = 
        oldInstance == nullptr || !oldInstance->settings->Equals(settings);

    if (shouldCreateNewInstance) {
        delete oldInstance;
        oldInstance = gcnew FrameAnalyzer(settings);
    }
}


FrameAnalyzer::FrameAnalyzer(AnalysisSettings^ settings) : settings(settings) {
    msclr::interop::marshal_context context;
    std::string gameNameConverted = context.marshal_as<std::string>(settings->GameName);
    std::wstring templatesDirectoryConverted = 
        context.marshal_as<std::wstring>(settings->TemplatesDirectory);
    SonicVisualSplitBase::AnalysisSettings nativeSettings = {gameNameConverted,
        templatesDirectoryConverted, settings->IsStretchedTo16By9, settings->IsComposite};

    auto nativeFrameAnalyzer = new SonicVisualSplitBase::FrameAnalyzer(nativeSettings);
    nativeFrameAnalyzerPtr = System::IntPtr(nativeFrameAnalyzer);
}


SonicVisualSplitBase::FrameAnalyzer* getFrameAnalyzerFromIntPtr(System::IntPtr ptr) {
    return static_cast<SonicVisualSplitBase::FrameAnalyzer*>(ptr.ToPointer());
}


FrameAnalyzer::~FrameAnalyzer() {
    delete getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr);
}


// Converting non-managed types to managed ones to call the native version of the function
AnalysisResult^ FrameAnalyzer::AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize) {
    auto nativeFrameAnalyzer = getFrameAnalyzerFromIntPtr(nativeFrameAnalyzerPtr);
    SonicVisualSplitBase::AnalysisResult result =
        nativeFrameAnalyzer->analyzeFrame(frameTime, checkForScoreScreen, visualize);

    AnalysisResult^ resultConverted = gcnew AnalysisResult();
    resultConverted->RecognizedTime = result.recognizedTime;
    resultConverted->TimeInMilliseconds = result.timeInMilliseconds;
    resultConverted->TimeString = gcnew String(result.timeString.c_str());
    resultConverted->IsScoreScreen = result.isScoreScreen;
    resultConverted->IsBlackScreen = result.isBlackScreen;
    resultConverted->IsWhiteScreen = result.isWhiteScreen;
    resultConverted->ErrorReason = (ErrorReasonEnum) result.errorReason;
    resultConverted->FrameTime = result.frameTime;

    if (resultConverted->ErrorReason != ErrorReasonEnum::VIDEO_DISCONNECTED && visualize) {
        // matrix to bitmap code taken from https://github.com/shimat/opencvsharp/blob/master/src/OpenCvSharp.Extensions/BitmapConverter.cs 
        const cv::Mat& mat = result.visualizedFrame;
        Bitmap^ converted = gcnew Bitmap(mat.cols, mat.rows, PixelFormat::Format24bppRgb);
        Rectangle rect(0, 0, mat.cols, mat.rows);
        BitmapData^ bitmapData = converted->LockBits(rect, ImageLockMode::WriteOnly, converted->PixelFormat);
        uint8_t* src = mat.data;
        uint8_t* dst = (uint8_t*) bitmapData->Scan0.ToPointer();
        int srcStep = (int) mat.step;
        int dstStep = bitmapData->Stride;

        for (int y = 0; y < mat.rows; y++) {
            long offsetSrc = (y * srcStep);
            long offsetDst = (y * dstStep);
            long bytesToCopy = mat.cols * 3;
            std::copy(src + offsetSrc, src + offsetSrc + bytesToCopy, dst + offsetDst);
        }

        converted->UnlockBits(bitmapData);
        resultConverted->VisualizedFrame = converted;
    }
    else {
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


void FrameStorage::SetVideoCapture(int sourceIndex) {
    SonicVisualSplitBase::FrameStorage::setVideoCapture(sourceIndex);
}


void FrameStorage::StartSavingFrames() {
    SonicVisualSplitBase::FrameStorage::startSavingFrames();
}


void FrameStorage::StopSavingFrames() {
    SonicVisualSplitBase::FrameStorage::stopSavingFrames();
}


List<Int64>^ FrameStorage::GetSavedFramesTimes() {
    std::vector<long long> savedFramesTimes = SonicVisualSplitBase::FrameStorage::getSavedFramesTimes();
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
    SonicVisualSplitBase::FrameStorage::deleteSavedFramesInRange(beginFrameTime, endFrameTime);
}


int FrameStorage::GetMaxCapacity() {
    return SonicVisualSplitBase::FrameStorage::MAX_CAPACITY;
}


Int64 FrameStorage::GetCurrentTimeInMilliseconds() {
    return SonicVisualSplitBase::FrameStorage::getCurrentTimeInMilliseconds();
}


String^ FrameStorage::GetVideoDisconnectedReason() {
    return gcnew String(SonicVisualSplitBase::FrameStorage::getVideoDisconnectedReason().c_str());
}


List<String^>^ VirtualCamCapture::GetVideoDevicesList() {
    std::vector<std::wstring> devices = SonicVisualSplitBase::VirtualCamCapture::getVideoDevicesList();
    List<String^>^ converted = gcnew List<String^>((int) devices.size());
    for (const std::wstring& deviceName : devices) {
        converted->Add(gcnew String(deviceName.c_str()));
    }
    return converted;
}

}  // namespace SonicVisualSplitWrapper