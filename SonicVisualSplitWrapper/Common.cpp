#include "Common.h"
#pragma managed(push, off)
#include "../SonicVisualSplitBase/VideoCaptureManager.h"
#include "../SonicVisualSplitBase/VirtualCamCapture.h"
#pragma managed(pop)
using namespace System;
using System::Collections::Generic::List;

namespace SonicVisualSplitWrapper {

AnalysisSettings::AnalysisSettings(SonicVisualSplitWrapper::Game game, String^ templatesDirectory,
    Boolean isStretchedTo16By9, Boolean isComposite) {
    Game = game;
    TemplatesDirectory = templatesDirectory;
    IsStretchedTo16By9 = isStretchedTo16By9;
    IsComposite = isComposite;
}


Boolean AnalysisSettings::Equals(Object^ other) {
    AnalysisSettings^ otherSettings = dynamic_cast<AnalysisSettings^>(other);
    return otherSettings &&
        otherSettings->Game == Game &&
        otherSettings->TemplatesDirectory == TemplatesDirectory &&
        otherSettings->IsStretchedTo16By9 == IsStretchedTo16By9 &&
        otherSettings->IsComposite == IsComposite;
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

}  // namespace SonicVisualSplitWrapper