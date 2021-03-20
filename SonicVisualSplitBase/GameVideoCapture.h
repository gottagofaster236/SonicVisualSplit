#pragma once
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    // Captures a game frame, which may need additional processing.
    virtual cv::Mat captureRawFrame() = 0;

    // Process the frame if needed. Returns an empty UMat in case of an error.
    virtual cv::UMat processFrame(cv::Mat rawFrame) {
        // Default implementation which just converts cv::Mat to cv::UMat.
        cv::UMat frame;
        rawFrame.copyTo(frame);
        return frame;
    }

    virtual ~GameVideoCapture() {}
};

} // namespace SonicVisualSplitBase