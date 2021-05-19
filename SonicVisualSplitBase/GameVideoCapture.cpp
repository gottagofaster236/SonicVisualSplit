#include "GameVideoCapture.h"
#include <opencv2/core.hpp>


namespace SonicVisualSplitBase {

cv::UMat SonicVisualSplitBase::GameVideoCapture::processFrame(cv::Mat rawFrame) {
    cv::UMat frame;
    rawFrame.copyTo(frame);
    return frame;
}


cv::Mat NullCapture::captureRawFrame() {
    // Returning an empty frame to indicate failure.
    return {};
}


std::chrono::milliseconds NullCapture::getDelayAfterLastFrame() {
    return std::chrono::milliseconds(100);
}

}  // namespace SonicVisualSplitBase