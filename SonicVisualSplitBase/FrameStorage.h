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

std::vector<long long> getSavedFramesTimes();

cv::UMat getSavedFrame(long long frameTime);

// Delete the frames whose save time is in the interval [beginFrameTime, endFrameTime)
void deleteSavedFramesInRange(long long beginFrameTime, long long endFrameTime);

// Caches the result of analyzing a frame for future usage.
void addResultToCache(const AnalysisResult& result);

// Writes the result to resultOutput if it's present in the cache, otherwise returns false.
bool getResultFromCache(long long frameTime, AnalysisResult& resultOutput);

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
