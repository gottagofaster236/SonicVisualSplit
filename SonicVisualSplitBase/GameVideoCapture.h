#pragma once
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    /* Captures a game frame, which may need additional processing.
     * Increments unsuccessfulFramesStreak on error. */
    cv::Mat captureRawFrame();

    /* Process the frame if needed. Returns an empty UMat in case of an error.
     * The default implementation just converts cv::Mat to cv::UMat. */
    virtual cv::UMat processFrame(cv::Mat rawFrame);

    int getUnsuccessfulFramesStreak();

    virtual ~GameVideoCapture() {}

private:
    virtual cv::Mat captureRawFrameImpl() = 0;

    int unsuccessfulFramesStreak = 0;
};

} // namespace SonicVisualSplitBase