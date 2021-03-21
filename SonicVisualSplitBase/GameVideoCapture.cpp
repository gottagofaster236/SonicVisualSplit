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
    cv::UMat frame;
    rawFrame.copyTo(frame);
    return frame;
}


int GameVideoCapture::getUnsuccessfulFramesStreak() {
    return unsuccessfulFramesStreak;
}

}  // namespace SonicVisualSplitBase