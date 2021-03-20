#pragma once
#include "GameVideoCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>


namespace SonicVisualSplitBase {

class VirtualCamCapture : public GameVideoCapture {
public:
    // Initializes the capture with a camera index.
    VirtualCamCapture(int cameraIndex);

    // Grabs a frame from the video capture, or returns an empty Mat on error.
    cv::Mat captureRawFrame() override;

    ~VirtualCamCapture() override;

private:
    cv::VideoCapture videoCapture;
};

}  // namespace SonicVisualSplitBase