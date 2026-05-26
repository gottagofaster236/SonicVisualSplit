#pragma once

#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {
namespace VideoCaptureManager {

/* Sets the video source.
 * If sourceIndex is non-negative, it'll treated as the index of a video source (i.e. webcam) in the system.
 * You can also pass NO_VIDEO_CAPTURE or OBS_WINDOW_CAPTURE. */
void setVideoCapture(int sourceIndex);

const int NO_VIDEO_CAPTURE = -1;  // No video capture is needed

// Gets a time in milliseconds since a certain time point.
long long getCurrentTimeInMilliseconds();

class OnSourceChangedListener {
public:
    virtual void onSourceChanged() = 0;
};

void addOnSourceChangedListener(OnSourceChangedListener& listener);

void removeOnSourceChangedListener(OnSourceChangedListener& listener);

struct CapturedFrame {
    cv::UMat frame;
    // See getCurrentTimeInMilliseconds()
    long long timestamp;
};

class OnFrameCapturedListener {
public:
    // Must not do any processing synchronously to avoid slowing down the capture
    virtual void onFrameCaptured(const CapturedFrame& capturedFrame) = 0;
};

void addOnFrameCapturedListener(OnFrameCapturedListener& listener);

void removeOnFrameCapturedListener(OnFrameCapturedListener& listener);

}  // namespace VideoCaptureManager
}  // namespace SonicVisualSplitBase
