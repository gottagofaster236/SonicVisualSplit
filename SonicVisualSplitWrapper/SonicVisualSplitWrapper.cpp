#include "SonicVisualSplitWrapper.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/FrameAnalyzer.h"
#include "../SonicVisualSplitBase/FrameStorage.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>
#include <algorithm>
#undef NO_ERROR  // defined in WinError.h
using System::Drawing::Imaging::PixelFormat;
using System::Drawing::Imaging::ImageLockMode;
using System::Drawing::Imaging::BitmapData;
using namespace SonicVisualSplitBase;

namespace SonicVisualSplitWrapper {

FrameAnalyzer::FrameAnalyzer(String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9)
    : gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9) {}


// Converting non-managed types to managed ones to call the native version of the function
AnalysisResult^ FrameAnalyzer::AnalyzeFrame(Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize, Boolean recalculateOnError) {
    msclr::interop::marshal_context context;
    std::string gameNameConverted = context.marshal_as<std::string>(gameName);
    std::wstring templatesDirectoryConverted = context.marshal_as<std::wstring>(templatesDirectory);
    auto& frameAnalyzer = SonicVisualSplitBase::FrameAnalyzer::getInstance(gameNameConverted, templatesDirectoryConverted, isStretchedTo16By9);
    SonicVisualSplitBase::AnalysisResult result = frameAnalyzer.analyzeFrame(frameTime, checkForScoreScreen, visualize, recalculateOnError);

    AnalysisResult^ resultConverted = gcnew AnalysisResult();
    resultConverted->RecognizedTime = result.recognizedTime;
    resultConverted->TimeInMilliseconds = result.timeInMilliseconds;
    resultConverted->TimeString = gcnew String(result.timeString.c_str());
    resultConverted->IsScoreScreen = result.isScoreScreen;
    resultConverted->IsBlackScreen = result.isBlackScreen;
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


Boolean AnalysisResult::IsSuccessful() {
    return ErrorReason == ErrorReasonEnum::NO_ERROR;
}


void AnalysisResult::MarkAsIncorrectlyRecognized() {
    FrameStorage::markFrameAsRecognizedIncorrectly(FrameTime);
}

void BaseWrapper::StartSavingFrames() {
    FrameStorage::startSavingFrames();
}


void BaseWrapper::StopSavingFrames() {
    FrameStorage::stopSavingFrames();
}


List<Int64>^ BaseWrapper::GetSavedFramesTimes() {
    std::vector<long long> savedFramesTimes = FrameStorage::getSavedFramesTimes();
    List<Int64>^ converted = gcnew List<Int64>(savedFramesTimes.size());
    for (long long frameTime : savedFramesTimes) {
        converted->Add(frameTime);
    }
    return converted;
}


void BaseWrapper::DeleteSavedFramesBefore(Int64 frameTime) {
    FrameStorage::deleteSavedFramesBefore(frameTime);
}

void BaseWrapper::DeleteAllSavedFrames() {
    FrameStorage::deleteAllSavedFrames();
}

}