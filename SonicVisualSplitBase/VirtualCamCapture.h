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

    static std::vector<std::wstring> getVideoDevicesList();

    // Should be called to cleanup the COM library, that is used by getVideoDevicesList.
    static void uninitialize();

private:
    // Grabs a frame from the video capture, or returns an empty Mat on error.
    cv::Mat captureRawFrameImpl() override;

    cv::VideoCapture videoCapture;

    // Returns the list of device names, which may contain duplicate names.
    static std::vector<std::wstring> getVideoDevicesListWithDuplicates();

    static inline bool hasInitializedCom = false;
};

}  // namespace SonicVisualSplitBase