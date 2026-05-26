#include "VideoCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace SonicVisualSplitBase {

cv::UMat VideoCapture::captureFrame() {
    cv::UMat frame = captureFrameImpl();
    if (frame.empty())
        unsuccessfulFramesStreak++;
    else
        unsuccessfulFramesStreak = 0;
    if (frame.rows > MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double)MAX_ACCEPTABLE_FRAME_HEIGHT) / frame.rows;
        cv::resize(frame, frame, {}, scaleFactor, scaleFactor, cv::INTER_AREA);
    }
    return frame;
}


int VideoCapture::getUnsuccessfulFramesStreak() {
    return unsuccessfulFramesStreak;
}


std::chrono::milliseconds NullCapture::getDelayAfterLastFrame() {
    return std::chrono::milliseconds(100);
}


cv::UMat NullCapture::captureFrameImpl() {
    // Returning an empty frame to indicate failure.
    return {};
}

}  // namespace SonicVisualSplitBase