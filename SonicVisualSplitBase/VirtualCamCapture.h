#pragma once
#include "GameVideoCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>
#include <string>


namespace SonicVisualSplitBase {

class VirtualCamCapture : public GameVideoCapture {
public:
    // Initializes the capture with a camera index.
    VirtualCamCapture(int deviceIndex);

    // Grabs a frame from the video capture, or returns an empty Mat on error.
    cv::Mat captureRawFrame() override;

    ~VirtualCamCapture() override;

    // Returns a vector of pairs: {device name, device index}.
    static std::vector<std::wstring> getVideoDevicesList();

private:
    cv::VideoCapture videoCapture;

    // Initializes Microsoft Media Foundation (needed by getVideoDevicesList), returns true on success.
    static bool loadMediaFoundation();

    static void unloadMediaFoundation();

    static inline bool hasLoadedMediaFoundation = false;
};

}  // namespace SonicVisualSplitBase