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


int GameVideoCapture::getUnsuccessfulFramesStreak() {
    return unsuccessfulFramesStreak;
}

}  // namespace SonicVisualSplitBase