#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include <functional>


namespace SonicVisualSplitBase {
namespace FrameStorage {

/* This module saves frames from the game video capture for further usage
 * (as we can't process all of them in real-time).
 * To distinguish between the frames, we use the time of capture
 * (in milliseconds since a certain point in time). */

 /* Sets the video source.
  * If sourceIndex is non-negative, it'll treated as the index of a video source (i.e. webcam) in the system.
  * You can also pass NO_VIDEO_CAPTURE or OBS_WINDOW_CAPTURE. */
void setVideoCapture(int sourceIndex);

const int NO_VIDEO_CAPTURE = -1;  // No video capture is needed
const int OBS_WINDOW_CAPTURE = -2;  // Capture the stream preview from currently opened OBS window.


// Creates a thread which saves a frame from the game video capture every 16 milliseconds.
void startSavingFrames();

// Stops that thread.
void stopSavingFrames();

// Returns the save times of the frames that are currently stored.
std::vector<long long> getSavedFramesTimes();

/* Returns a saved frame by the frame save time, or an empty cv::UMat if:
 * (1) video was disconnected at the time of capture or
 * (2) the frame storage doesn't contain this frame. */
cv::UMat getSavedFrame(long long frameTime);

/* Same as getSavedFrame().
 * Used to avoid the race condition when you first get 
 * the last frame time and **then** call getSavedFrame(frameTime). */
cv::UMat getLastSavedFrame();

// Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime);

// Gets a time in milliseconds since a certain time point.
long long getCurrentTimeInMilliseconds();

// Returns the reason why the video is currently disconnected, or an empty string if not available.
std::string getVideoDisconnectedReason();

class OnSourceChangedListener {
public:
    virtual void onSourceChanged() const = 0;
};

void addOnSourceChangedListener(const OnSourceChangedListener& listener);

void removeOnSourceChangedListener(const OnSourceChangedListener& listener);

/* The maximum amount of frames that can be saved in the storage.
 * When this limit is reached, no new frames are saved until deleteSavedFramesInRange is called. */
const int MAX_CAPACITY = 240;

}  // namespace FrameStorage
}  // namespace SonicVisualSplitBase
