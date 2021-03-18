#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include "FrameAnalyzer.h"

namespace SonicVisualSplitBase {
namespace FrameStorage {

/* This module saves frames from OBS for further usage (as we can't process all of them in real-time).
 * To distinguish between the frames, we use the time of capture (in milliseconds from epoch). */

// Creates a thread which saves a screenshot of OBS every 16 milliseconds.
void startSavingFrames();

void stopSavingFrames();

// Returns the save times of the frames that are currently stored.
std::vector<long long> getSavedFramesTimes();

/* Returns a saved frame by the frame save time,
 * or an empty cv::UMat if video was disconnected at the time of capture. */
cv::UMat getSavedFrame(long long frameTime);

// Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime);

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
