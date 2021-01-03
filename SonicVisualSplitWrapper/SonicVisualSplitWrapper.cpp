#include "SonicVisualSplitWrapper.h"
#include "../SonicVisualSplitBase/FrameAnalyzer.h"
#include "../SonicVisualSplitBase/FrameStorage.h"
#include <msclr/marshal_cppstd.h>
#include <algorithm>
using System::Drawing::Imaging::PixelFormat;
using System::Drawing::Imaging::ImageLockMode;
using System::Drawing::Imaging::BitmapData;
using namespace SonicVisualSplitBase;

namespace SonicVisualSplitWrapper {

// Converting non-managed types to managed ones to call the native version of the function
AnalysisResult^ BaseWrapper::AnalyzeFrame(String^ gameName, String^ templatesDirectory, Boolean isStretchedTo16By9,
                                          Int64 frameTime, Boolean checkForScoreScreen, Boolean visualize, Boolean recalculateOnError) {
    msclr::interop::marshal_context context;
    std::string gameNameConverted = context.marshal_as<std::string>(gameName);
    std::wstring templatesDirectoryConverted = context.marshal_as<std::wstring>(templatesDirectory);
    SonicVisualSplitBase::FrameAnalyzer& frameAnalyzer = FrameAnalyzer::getInstance(gameNameConverted, templatesDirectoryConverted, isStretchedTo16By9);
    SonicVisualSplitBase::AnalysisResult result = frameAnalyzer.analyzeFrame(frameTime, checkForScoreScreen, visualize, recalculateOnError);

    AnalysisResult^ resultConverted = gcnew AnalysisResult();
    resultConverted->FoundAnyDigits = result.foundAnyDigits;
    resultConverted->TimeDigits = gcnew String(result.timeDigits.c_str());
    resultConverted->IsScoreScreen = result.isScoreScreen;
    resultConverted->IsBlackScreen = result.isBlackScreen;
    resultConverted->ErrorReason = (ErrorReasonEnum) result.errorReason;
    if (resultConverted->ErrorReason != ErrorReasonEnum::VIDEO_DISCONNECTED && visualize) {
        const cv::Mat& mat = result.visualizedFrame;
        // matrix to bitmap code taken from https://github.com/shimat/opencvsharp/blob/master/src/OpenCvSharp.Extensions/BitmapConverter.cs 
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


void BaseWrapper::StartSavingFrames() {
    FrameStorage::startSavingFrames();
}


List<Int64>^ BaseWrapper::GetSavedFramesTimes() {
    std::vector<long long> savedFramesTimes = FrameStorage::getSavedFramesTimes();
    List<Int64>^ converted = gcnew List<Int64>(savedFramesTimes.size());
    for (int i = 0; i < savedFramesTimes.size(); i++) {
        converted[i] = savedFramesTimes[i];
    }
    return converted;
}


void BaseWrapper::DeleteSavedFramesBefore(Int64 frameTime) {
    FrameStorage::deleteSavedFramesBefore(frameTime);
}

}