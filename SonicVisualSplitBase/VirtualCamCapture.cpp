#include "VirtualCamCapture.h"
#include <dshow.h>


namespace SonicVisualSplitBase {

VirtualCamCapture::VirtualCamCapture(int deviceIndex) {
    videoCapture = cv::VideoCapture(deviceIndex, cv::CAP_DSHOW);
    // Set the highest possible resolution. (Assuming it's less than 32000 pixels).
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, 32000);
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 32000);
}


cv::Mat VirtualCamCapture::captureRawFrameImpl() {
    cv::Mat frame;
    if (!videoCapture.isOpened())
        return frame;  // Return an empty cv::Mat on error.
    videoCapture >> frame;
    return frame;
}


VirtualCamCapture::~VirtualCamCapture() {
    videoCapture.release();
}


// Code for device enumeration taken from here: https://docs.microsoft.com/en-us/windows/win32/directshow/selecting-a-capture-device
static HRESULT EnumerateDevices(REFGUID category, IEnumMoniker** ppEnum) {
    // Create the System Device Enumerator.
    ICreateDevEnum* pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr)) {
        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE)         {
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
        }
        pDevEnum->Release();
    }
    return hr;
}


std::vector<std::wstring> VirtualCamCapture::getVideoDevicesListWithDuplicates() {
    if (!hasInitializedCom) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr))
            return {};
        hasInitializedCom = true;
    }

    std::vector<std::wstring> devices;
    IEnumMoniker* pEnum;
    HRESULT hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);

    if (SUCCEEDED(hr)) {
        IMoniker* pMoniker = NULL;

        while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
            IPropertyBag* pPropBag;
            HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
            auto failString = L"Unknown";

            if (FAILED(hr)) {
                pMoniker->Release();
                devices.push_back(failString);
                continue;
            }

            VARIANT var;
            VariantInit(&var);
            // Get the friendly name.
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
            if (SUCCEEDED(hr)) {
                devices.push_back(std::wstring(var.bstrVal));
                VariantClear(&var);
            }
            else {
                devices.push_back(failString);
            }

            pPropBag->Release();
            pMoniker->Release();
        }

        pEnum->Release();
    }
    return devices;
}


std::vector<std::wstring> VirtualCamCapture::getVideoDevicesList() {
    // Removing the duplicates by adding the duplicate number.
    std::vector<std::wstring> devicesWithDuplicates = getVideoDevicesListWithDuplicates();
    std::vector<std::wstring> devices;

    for (const std::wstring& name : devicesWithDuplicates) {
        std::wstring addName = name;
        int repetition = 1;
        while (std::find(devices.begin(), devices.end(), addName) != devices.end()) {
            repetition++;
            addName = name + L" (" + std::to_wstring(repetition) + L")";
        }
        devices.push_back(addName);
    }
    return devices;
}


void VirtualCamCapture::uninitialize() {
    CoUninitialize();
    hasInitializedCom = false;
}

}  // namespace SonicVisualSplitBase