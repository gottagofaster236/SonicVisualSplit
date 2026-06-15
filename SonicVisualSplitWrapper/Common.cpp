#include "Common.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/VideoCaptureManager.h"
#include "../SonicVisualSplitBase/VirtualCamCapture.h"
#pragma managed(pop)
#include <msclr/marshal_cppstd.h>
using namespace System;
using System::Drawing::Imaging::PixelFormat;
using System::Drawing::Imaging::ImageLockMode;
using System::Drawing::Imaging::BitmapData;
using System::Drawing::Bitmap;
using System::Collections::Generic::List;

namespace SonicVisualSplitWrapper {

AnalysisSettings::AnalysisSettings(SonicVisualSplitWrapper::Game game, String^ templatesDirectory,
    Boolean isStretchedTo16By9, Boolean isComposite, Boolean autoResetEnabled) {
    Game = game;
    TemplatesDirectory = templatesDirectory;
    IsStretchedTo16By9 = isStretchedTo16By9;
    IsComposite = isComposite;
    AutoResetEnabled = autoResetEnabled;
}


Boolean AnalysisSettings::Equals(Object^ other) {
    AnalysisSettings^ otherSettings = dynamic_cast<AnalysisSettings^>(other);
    return otherSettings &&
        otherSettings->Game == Game &&
        otherSettings->TemplatesDirectory == TemplatesDirectory &&
        otherSettings->IsStretchedTo16By9 == IsStretchedTo16By9 &&
        otherSettings->IsComposite == IsComposite &&
        otherSettings->AutoResetEnabled == AutoResetEnabled;
}

AnalysisSettings::operator SonicVisualSplitBase::AnalysisSettings() {
    msclr::interop::marshal_context context;
    std::wstring templatesDirectoryConverted =
        context.marshal_as<std::wstring>(TemplatesDirectory);
    return {static_cast<SonicVisualSplitBase::Game>(Game), templatesDirectoryConverted, IsStretchedTo16By9, IsComposite, AutoResetEnabled};
}


void VideoCaptureManager::SetVideoCapture(int sourceIndex) {
    SonicVisualSplitBase::VideoCaptureManager::setVideoCapture(sourceIndex);
}


Int64 VideoCaptureManager::GetCurrentTimeInMilliseconds() {
    return SonicVisualSplitBase::VideoCaptureManager::getCurrentTimeInMilliseconds();
}


List<String^>^ VirtualCamCapture::GetVideoDevicesList() {
    std::vector<std::wstring> devices = SonicVisualSplitBase::VirtualCamCapture::getVideoDevicesList();
    List<String^>^ converted = gcnew List<String^>((int) devices.size());
    for (const std::wstring& deviceName : devices) {
        converted->Add(gcnew String(deviceName.c_str()));
    }
    return converted;
}


System::Drawing::Bitmap^ toBitmap(const cv::Mat& mat) {
    // cv::Mat to Bitmap code taken from https://github.com/shimat/opencvsharp/blob/ba919eab93bf56c44332cd8eb326db3b50b11c88/src/OpenCvSharp.Extensions/BitmapConverter.cs
    // (c) shimat,vladkol, Apache 2.0 License
    Bitmap^ converted = gcnew Bitmap(mat.cols, mat.rows, PixelFormat::Format24bppRgb);
    System::Drawing::Rectangle rect(0, 0, mat.cols, mat.rows);
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
    return converted;
}

}  // namespace SonicVisualSplitWrapper
