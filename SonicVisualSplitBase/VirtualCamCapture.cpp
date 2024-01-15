#include "VirtualCamCapture.h"
#include "TimeRecognizer.h"
#include <ranges>
#include <algorithm>


namespace SonicVisualSplitBase {

VirtualCamCapture::VirtualCamCapture(int deviceIndex) {
    videoCapture = cv::VideoCapture(deviceIndex, cv::CAP_DSHOW);
    cv::Size resolution = getResolution(deviceMonikers[deviceIndex]);
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);
    videoCapture.set(cv::CAP_PROP_FPS, 60);
}


cv::Mat VirtualCamCapture::captureRawFrameImpl() {
    cv::Mat frame;
    if (!videoCapture.isOpened())
        return {};
    videoCapture >> frame;
    return frame;
}


VirtualCamCapture::~VirtualCamCapture() {
    videoCapture.release();
}


std::vector<std::wstring> VirtualCamCapture::getVideoDevicesList() {
    enumerateDeviceMonikers();
    auto devices = std::views::transform(deviceMonikers, getName);
    return std::vector<std::wstring>(devices.begin(), devices.end());
}


std::chrono::milliseconds VirtualCamCapture::getDelayAfterLastFrame() {
    if (getUnsuccessfulFramesStreak() > 0) {
        // The last frame was a fail, just wait for the next one.
        return std::chrono::milliseconds(1000) / 60;
    }
    else {
        // cv::VideoCapture waits for the next frame by itself, no delay is needed.
        return std::chrono::milliseconds(0);
    }
}


void VirtualCamCapture::enumerateDeviceMonikers() {
    deviceMonikers.clear();

    if (!initializeCom())
        return;

    CComPtr<IEnumMoniker> enumMoniker;
    HRESULT hr = createDeviceEnumMoniker(CLSID_VideoInputDeviceCategory, &enumMoniker);
    if (FAILED(hr))
        return;

    while (true) {
        CComPtr<IMoniker> moniker;
        if (enumMoniker->Next(1, &moniker, nullptr) != S_OK)
            break;
        deviceMonikers.push_back(moniker);
    }
}


// Code for device enumeration taken from here: https://docs.microsoft.com/en-us/windows/win32/directshow/selecting-a-capture-device
HRESULT VirtualCamCapture::createDeviceEnumMoniker(REFGUID category, IEnumMoniker** ppEnum) {
    // Create the System Device Enumerator.
    CComPtr<ICreateDevEnum> devEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum));
    if (FAILED(hr))
        return hr;

    // Create an enumerator for the category.
    hr = devEnum->CreateClassEnumerator(category, ppEnum, 0);
    if (hr == S_FALSE) {
        hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
    }
    return hr;
}


std::wstring VirtualCamCapture::getName(IMoniker* moniker) {
    auto failString = L"Unknown";

    CComPtr<IPropertyBag> propBag;
    HRESULT hr = moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&propBag));
    if (FAILED(hr))
        return failString;

    // Get the friendly name.
    CComVariant var;
    hr = propBag->Read(L"FriendlyName", &var, 0);

    if (SUCCEEDED(hr)) {
        std::wstring name(var.bstrVal);
        return name;
    }
    else {
        return failString;
    }
}


// Release the format block for a media type.
void _FreeMediaType(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat != 0) {
        CoTaskMemFree((PVOID) mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }
    if (mt.pUnk != nullptr) {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}


// Delete a media type structure that was allocated on the heap.
void _DeleteMediaType(AM_MEDIA_TYPE* pmt) {
    if (pmt != nullptr) {
        _FreeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}


cv::Size VirtualCamCapture::getResolution(IMoniker* moniker) {
    std::vector<cv::Size> supportedResolutions = getSupportedResolutions(moniker);
    if (supportedResolutions.empty())
        return {0, 0};
    auto hdResolutions = supportedResolutions | std::views::filter([](const auto& resolution) {
        return resolution.height >= TimeRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT;
    });

    if (hdResolutions.empty()) {
        // If all of the resolutions are low-res, return the resolution with the highest area (width * height).
        cv::Size maxResolution = std::ranges::max(supportedResolutions, {}, &cv::Size::area);
        return maxResolution;
    }
    else {
        // Find the widest aspect ratio, and return the smallest hi-res resolution with that aspect ratio.
        double widestAspectRatio = std::ranges::max(
            std::views::transform(hdResolutions, &cv::Size::aspectRatio));
        auto wideResolutions = hdResolutions | std::views::filter([&](const auto& resolution) {
            return resolution.aspectRatio() >= widestAspectRatio * 0.98;
        });
        cv::Size minResolution = std::ranges::min(wideResolutions, {}, &cv::Size::area);
        return minResolution;
    }
}


std::vector<cv::Size> VirtualCamCapture::getSupportedResolutions(IMoniker* moniker) {
    std::vector<cv::Size> resolutions;
    HRESULT hr;

    // Binding the moniker to a base filter.
    CComPtr<IBaseFilter> filter;
    hr = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**) &filter);
    if (FAILED(hr))
        return {};
    
    // Enumerating over the pins of the base filter.
    CComPtr<IEnumPins> enumPins;
    hr = filter->EnumPins(&enumPins);
    if (FAILED(hr))
        return {};

    while (true) {
        CComPtr<IPin> pin;
        if (enumPins->Next(1, &pin, nullptr) != S_OK) {
            break;
        }

        // Enumurating over the media types of the pin.
        CComPtr<IEnumMediaTypes> enumMediaTypes;
        hr = pin->EnumMediaTypes(&enumMediaTypes);
        if (FAILED(hr))
            continue;

        AM_MEDIA_TYPE* mediaType;
        while (enumMediaTypes->Next(1, &mediaType, nullptr) == S_OK) {
            if (mediaType->formattype == FORMAT_VideoInfo &&
                mediaType->cbFormat >= sizeof(VIDEOINFOHEADER) &&
                mediaType->pbFormat) {
                VIDEOINFOHEADER* videoInfoHeader = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
                int width = videoInfoHeader->bmiHeader.biWidth;
                int height = videoInfoHeader->bmiHeader.biHeight;
                cv::Size resolution(width, height);
                resolutions.push_back(resolution);
            }
            _DeleteMediaType(mediaType);
        }
    }

    return resolutions;
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
