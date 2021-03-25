#pragma once
#include "GameVideoCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>


namespace SonicVisualSplitBase {

class VirtualCamCapture : public GameVideoCapture {
public:
    // Initializes the capture with a camera index.
    VirtualCamCapture(int deviceIndex);

    ~VirtualCamCapture() override;

    /* Returns the list of device names.
     * May contain duplicate names, if there are several devices with the same name. */
    static std::vector<std::wstring> getVideoDevicesList();

private:
    // Grabs a frame from the video capture, or returns an empty Mat in case of error.
    cv::Mat captureRawFrameImpl() override;

    cv::VideoCapture videoCapture;

    static std::vector<std::wstring> getVideoDevicesListWithDuplicates();

    static bool initializeCom();
};

}  // namespace SonicVisualSplitBase