#pragma once
#include <opencv2/core.hpp>
#include <chrono>


namespace SonicVisualSplitBase {

// An abstract class for video capture.
class VideoCapture {
public:
    /* Captures a game frame.
     * Increments unsuccessfulFramesStreak in case of error. */
    cv::UMat captureFrame();

    // Gets the duration of time to wait before capturing the next frame.
    virtual std::chrono::milliseconds getDelayAfterLastFrame() = 0;

    int getUnsuccessfulFramesStreak();

    virtual ~VideoCapture() {}

private:
    virtual cv::UMat captureFrameImpl() = 0;

    int unsuccessfulFramesStreak = 0;
};


// Using null-object pattern to handle the situation where there's no capture specified.
class NullCapture : public VideoCapture {
public:
    std::chrono::milliseconds getDelayAfterLastFrame() override;

private:
    cv::UMat captureFrameImpl() override;
};

} // namespace SonicVisualSplitBase
