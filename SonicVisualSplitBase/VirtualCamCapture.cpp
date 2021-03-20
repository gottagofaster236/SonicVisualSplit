#include "VirtualCamCapture.h"
#include <mfapi.h>
#include <Mfidl.h>
#include <cstdlib>
#include <algorithm>

namespace SonicVisualSplitBase {

VirtualCamCapture::VirtualCamCapture(int deviceIndex) {
    videoCapture = cv::VideoCapture(deviceIndex, cv::CAP_MSMF);
    // Set the highest possible resolution. (Assuming it's less than 32000 pixels).
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, 32000);
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 32000);
}


cv::Mat VirtualCamCapture::captureRawFrame() {
    cv::Mat frame;
    if (!videoCapture.isOpened())
        return frame;  // Return an empty cv::Mat on error.
    videoCapture >> frame;
    return frame;
}


VirtualCamCapture::~VirtualCamCapture() {
    videoCapture.release();
}


std::vector<std::wstring> VirtualCamCapture::getVideoDevicesList() {
    if (!loadMediaFoundation())
        return {};
    std::vector<std::wstring> videoDevices;

    // Create an attribute store to specify the enumeration parameters.
    IMFAttributes* pAttributes = nullptr;
    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );

    // Enumerate devices.
    UINT32 count = 0;
    IMFActivate** ppDevices = nullptr;
    HRESULT enumerationResult = MFEnumDeviceSources(pAttributes, &ppDevices, &count);

    if (SUCCEEDED(enumerationResult)) {
        for (unsigned int i = 0; i < count; i++) {
            std::wstring friendlyName;

            WCHAR* szFriendlyName = nullptr;
            UINT32 length;
            HRESULT result = ppDevices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                &szFriendlyName,
                &length
            );
            
            if (FAILED(result)) {
                friendlyName = L"Unknown";
            }
            else {
                friendlyName = std::wstring(szFriendlyName);
                CoTaskMemFree(szFriendlyName);
            }

            std::wstring addName = friendlyName;
            int repetition = 1;
            while (std::find(videoDevices.begin(), videoDevices.end(), addName) != videoDevices.end()) {
                repetition++;
                addName = friendlyName + L" (" + std::to_wstring(repetition) + L")";
            }
            videoDevices.push_back(addName);
        }
    }

    pAttributes->Release();
    for (unsigned int i = 0; i < count; i++) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    return videoDevices;
}


bool VirtualCamCapture::loadMediaFoundation() {
    if (hasLoadedMediaFoundation)
        return true;

    HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(result))
        return false;
    
    hasLoadedMediaFoundation = true;
    std::atexit(unloadMediaFoundation);
    return true;
}


void VirtualCamCapture::unloadMediaFoundation() {
    MFShutdown();
}


}  // namespace SonicVisualSplitBase