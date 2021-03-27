#include "VirtualCamCapture.h"
#include <dshow.h>


namespace SonicVisualSplitBase {

VirtualCamCapture::VirtualCamCapture(int deviceIndex) {
    videoCapture = cv::VideoCapture(deviceIndex, cv::CAP_DSHOW);
    cv::Size maximumResolution = maximumCaptureResolutions[deviceIndex];
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, maximumResolution.width);
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, maximumResolution.height);
}


cv::Mat VirtualCamCapture::captureRawFrameImpl() {
    cv::Mat frame;
    if (!videoCapture.isOpened())
        return frame;  // Return an empty cv::Mat in case of error.
    videoCapture >> frame;
    return frame;
}


VirtualCamCapture::~VirtualCamCapture() {
    videoCapture.release();
}


std::vector<std::wstring> VirtualCamCapture::getVideoDevicesList() {
    std::vector<std::wstring> devices;
    maximumCaptureResolutions.clear();

    if (!initializeCom())
        return {};

    IEnumMoniker* enumMoniker;
    HRESULT hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &enumMoniker);
    if (SUCCEEDED(hr)) {
        IMoniker* moniker;

        while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
            devices.push_back(getName(moniker));
            maximumCaptureResolutions.push_back(getMaxResolution(moniker));
            moniker->Release();
        }

        enumMoniker->Release();
    }
    return devices;
}


// Code for device enumeration taken from here: https://docs.microsoft.com/en-us/windows/win32/directshow/selecting-a-capture-device
HRESULT VirtualCamCapture::EnumerateDevices(REFGUID category, IEnumMoniker** ppEnum) {
    // Create the System Device Enumerator.
    ICreateDevEnum* devEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum));

    if (SUCCEEDED(hr)) {
        // Create an enumerator for the category.
        hr = devEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE) {
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
        }
        devEnum->Release();
    }
    return hr;
}


std::wstring VirtualCamCapture::getName(IMoniker* pMoniker) {
    auto failString = L"Unknown";

    IPropertyBag* propBag;
    HRESULT hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&propBag));
    if (FAILED(hr))
        return failString;

    // Get the friendly name.
    VARIANT var;
    VariantInit(&var);
    hr = propBag->Read(L"FriendlyName", &var, 0);
    propBag->Release();

    if (SUCCEEDED(hr)) {
        std::wstring name(var.bstrVal);
        VariantClear(&var);
        return name;
    }
    else {
        return failString;
    }
}


// Release the format block for a media type.
void _FreeMediaType(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat != 0)     {
        CoTaskMemFree((PVOID) mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }
    if (mt.pUnk != nullptr)     {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}


// Delete a media type structure that was allocated on the heap.
void _DeleteMediaType(AM_MEDIA_TYPE* pmt) {
    if (pmt != nullptr)     {
        _FreeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}


cv::Size VirtualCamCapture::getMaxResolution(IMoniker* pMoniker) {
    cv::Size maxResolution(0, 0);
    HRESULT hr;

    // Binding the moniker to a base filter.
    IBaseFilter* filter;
    hr = pMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**) &filter);
    if (FAILED(hr)) {
        // Return an empty resolution in case of error.
        return maxResolution;
    }
    
    // Enumerating over the pins of the base filter.
    IEnumPins* pEnumPins;
    hr = filter->EnumPins(&pEnumPins);

    if (SUCCEEDED(hr)) {
        IPin* pin;
        while (pEnumPins->Next(1, &pin, nullptr) == S_OK) {
            // Enumurating over the media types of the pin.
            IEnumMediaTypes* enumMediaTypes;
            hr = pin->EnumMediaTypes(&enumMediaTypes);
            if (FAILED(hr)) {
                pin->Release();
                continue;
            }
            
            AM_MEDIA_TYPE* mediaType;
            while (enumMediaTypes->Next(1, &mediaType, nullptr) == S_OK) {
                if (mediaType->formattype == FORMAT_VideoInfo &&
                        mediaType->cbFormat >= sizeof(VIDEOINFOHEADER) &&
                        mediaType->pbFormat) {
                    VIDEOINFOHEADER* videoInfoHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
                    int width = videoInfoHeader->bmiHeader.biWidth;
                    int height = videoInfoHeader->bmiHeader.biHeight;
                    cv::Size resolution(width, height);
                    if (resolution.area() > maxResolution.area())
                        maxResolution = resolution;
                }
                _DeleteMediaType(mediaType);
            }

            enumMediaTypes->Release();
            pin->Release();
        }

        pEnumPins->Release();
    }
    filter->Release();
    return maxResolution;
}


class ComInitializer {
public:
    ComInitializer() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        succeded = SUCCEEDED(hr);
    }

    ~ComInitializer() {
        CoUninitialize();
    }

    bool succeded = false;
};


bool VirtualCamCapture::initializeCom() {
    thread_local ComInitializer comInitializer;
    return comInitializer.succeded;
}

}  // namespace SonicVisualSplitBase