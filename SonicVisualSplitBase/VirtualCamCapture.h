#pragma once
#include "GameVideoCapture.h"
#include "CapDShowFixed.h"
#include <opencv2/core.hpp>
#define NOMINMAX
#include <dshow.h>
#include <vector>
#include <string>
#include <utility>
#include <atlbase.h>


namespace SonicVisualSplitBase {

class VirtualCamCapture : public GameVideoCapture {
public:
    // Initializes the capture with a camera index.
    VirtualCamCapture(int deviceIndex);

    /* Returns the list of device names.
     * May contain duplicate names, if there are several devices with the same name. */
    static std::vector<std::wstring> getVideoDevicesList();

    std::chrono::milliseconds getDelayAfterLastFrame() override;

private:
    // Grabs a frame from the video capture, or returns an empty Mat in case of error.
    cv::Mat captureRawFrameImpl() override;

    cvFixed::VideoCapture_DShow videoCapture;

    inline static std::vector<CComPtr<IMoniker>> deviceMonikers;

    static void enumerateDeviceMonikers();
    static HRESULT createDeviceEnumMoniker(REFGUID category, IEnumMoniker** ppEnum);
    static std::wstring getName(IMoniker* moniker);
    static cv::Size getResolution(IMoniker* moniker);
    static std::vector<cv::Size> getSupportedResolutions(IMoniker* moniker);
    static bool initializeCom();
};

}  // namespace SonicVisualSplitBase