#pragma once
#include <vector>
#ifdef __cplusplus_cli
#pragma managed(push, off)
#endif
#include <opencv2/opencv.hpp>
#ifdef __cplusplus_cli
#pragma managed(pop)
#endif

namespace SonicVisualSplitBase {
namespace FrameStorage {

// This module saves frames from OBS for further usage.
// To distinguish between the frames, we use the capture time in milliseconds from epoch.

void startSavingFrames();

std::vector<long long> getSavedFramesTimes();

cv::UMat getSavedFrame(long long frameTime);

void deleteSavedFramesBefore(long long frameTime);

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
