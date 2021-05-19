#pragma once
#include "GameVideoCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#define NOMINMAX
#include <dshow.h>
#include <vector>
#include <string>
#include <utility>


namespace SonicVisualSplitBase {

class VirtualCamCapture : public GameVideoCapture {
public:
    // Initializes the capture with a camera index.
    VirtualCamCapture(int deviceIndex);

    ~VirtualCamCapture() override;

    // Grabs a frame from the video capture, or returns an empty Mat in case of error.
    cv::Mat captureRawFrame() override;

    /* Returns the list of device names.
     * May contain duplicate names, if there are several devices with the same name. */
    static std::vector<std::wstring> getVideoDevicesList();

    std::chrono::milliseconds getDelayAfterLastFrame() override;

private:
    cv::VideoCapture videoCapture;

    bool isLastFrameSuccessful = true;

    inline static std::vector<cv::Size> captureResolutions;

    static HRESULT EnumerateDevices(REFGUID category, IEnumMoniker** ppEnum);
    static std::wstring getName(IMoniker* moniker);
    static cv::Size getResolution(IMoniker* moniker);
    static std::vector<cv::Size> getSupportedResolutions(IMoniker* moniker);
    static bool initializeCom();
};

}  // namespace SonicVisualSplitBase