#pragma once
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
    // Captures a game frame, which may need additional processing.
    virtual cv::UMat captureRawFrame() = 0;

    // Process the frame if needed.
    virtual inline cv::UMat processFrame(cv::UMat frame) {
        // Default implementation which does nothing.
        return frame;
    }
};

} // namespace SonicVisualSplitBase