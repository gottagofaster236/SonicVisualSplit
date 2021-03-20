#include "VirtualCamCapture.h"

namespace SonicVisualSplitBase {

VirtualCamCapture::VirtualCamCapture(int cameraIndex) {
    videoCapture = cv::VideoCapture(cameraIndex, cv::CAP_DSHOW);
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

}  // namespace SonicVisualSplitBase