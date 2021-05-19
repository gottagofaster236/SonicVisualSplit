#pragma once
#include <opencv2/core.hpp>
#include <chrono>


namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    /* Captures a game frame, which may need additional processing.
     * Increments unsuccessfulFramesStreak in case of error. */
    virtual cv::Mat captureRawFrame() = 0;

    /* Process the frame if needed. Returns an empty UMat in case of an error.
     * The default implementation just converts cv::Mat to cv::UMat. */
    virtual cv::UMat processFrame(cv::Mat rawFrame);

    // Gets the duration of time to wait before capturing the next frame.
    virtual std::chrono::milliseconds getDelayAfterLastFrame() = 0;

    virtual ~GameVideoCapture() {}
};


// Using null-object pattern to handle the situation where there's no capture specified.
class NullCapture : public GameVideoCapture {
public:
    cv::Mat captureRawFrame() override;

    std::chrono::milliseconds getDelayAfterLastFrame() override;
};

} // namespace SonicVisualSplitBase