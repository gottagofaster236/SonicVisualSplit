#include "GameVideoCapture.h"
#include <opencv2/core.hpp>


namespace SonicVisualSplitBase {

cv::Mat GameVideoCapture::captureRawFrame() {
    cv::Mat frame = captureRawFrameImpl();
    if (frame.empty())
        unsuccessfulFramesStreak++;
    else
        unsuccessfulFramesStreak = 0;
    return frame;
}


cv::UMat SonicVisualSplitBase::GameVideoCapture::processFrame(cv::Mat rawFrame) {
    return rawFrame.getUMat(cv::ACCESS_READ);
}


std::string GameVideoCapture::getVideoDisconnectedReason() {
    return std::string();
}


int GameVideoCapture::getUnsuccessfulFramesStreak() {
    return unsuccessfulFramesStreak;
}


std::chrono::milliseconds NullCapture::getDelayAfterLastFrame() {
    return std::chrono::milliseconds(100);
}


cv::Mat NullCapture::captureRawFrameImpl() {
    // Returning an empty frame to indicate failure.
    return {};
}

}  // namespace SonicVisualSplitBase