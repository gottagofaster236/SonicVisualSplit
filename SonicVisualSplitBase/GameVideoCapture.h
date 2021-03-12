#pragma once
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {

// An abstract class for video capture.
class GameVideoCapture {
public:
    // Captures a game frame, which may need additional processing.
    virtual cv::Mat captureRawFrame() = 0;

    // Process the frame if needed. Returns an empty UMat in case of an error.
    virtual cv::UMat processFrame(cv::Mat rawFrame) = 0;

    virtual ~GameVideoCapture() {}
};

} // namespace SonicVisualSplitBase