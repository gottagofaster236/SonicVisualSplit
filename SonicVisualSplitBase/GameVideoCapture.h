#pragma once
#include <opencv2/core.hpp>
#include <chrono>


namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    /* Captures a game frame, which may need additional processing.
     * Increments unsuccessfulFramesStreak in case of error. */
    cv::Mat captureRawFrame();

    /* Process the frame if needed. Returns an empty UMat in case of an error.
     * The default implementation just converts cv::Mat to cv::UMat. */
    virtual cv::UMat processFrame(cv::Mat rawFrame);

    // Gets the duration of time to wait before capturing the next frame.
    virtual std::chrono::milliseconds getDelayAfterLastFrame() = 0;

    /* Returns the reason why the video is currently disconnected,
     * or an empty string if not available. */
    virtual std::string getVideoDisconnectedReason();

    int getUnsuccessfulFramesStreak();

    virtual ~GameVideoCapture() {}

private:
    virtual cv::Mat captureRawFrameImpl() = 0;

    int unsuccessfulFramesStreak = 0;
};


// Using null-object pattern to handle the situation where there's no capture specified.
class NullCapture : public GameVideoCapture {
public:
    std::chrono::milliseconds getDelayAfterLastFrame() override;

private:
    cv::Mat captureRawFrameImpl() override;
};

} // namespace SonicVisualSplitBase