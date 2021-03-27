#pragma once
#include <opencv2/core.hpp>


namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    /* Captures a game frame, which may need additional processing.
     * Increments unsuccessfulFramesStreak in case of error. */
    cv::Mat captureRawFrame();

    /* Process the frame if needed. Returns an empty UMat in case of an error.
     * The default implementation just converts cv::Mat to cv::UMat. */
    virtual cv::UMat processFrame(cv::Mat rawFrame) = 0;

    int getUnsuccessfulFramesStreak();

    virtual ~GameVideoCapture() {}

    /* The implementations should make sure that the frame returned by processFrame is not too large.
     * This helps increase the speed of frame analyzation. */
    const int MAXIMUM_ACCEPTABLE_FRAME_HEIGHT = 600;

private:
    virtual cv::Mat captureRawFrameImpl() = 0;

    int unsuccessfulFramesStreak = 0;
};

} // namespace SonicVisualSplitBase