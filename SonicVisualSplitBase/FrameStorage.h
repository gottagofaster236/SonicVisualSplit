#pragma once
#include <vector>
#ifdef __cplusplus_cli
#pragma managed(push, off)
#endif
#include <opencv2/core.hpp>
#ifdef __cplusplus_cli
#pragma managed(pop)
#endif

namespace SonicVisualSplitBase {
namespace FrameStorage {

// This module saves frames from OBS for further usage (as we can't process all of them in real-time).
// To distinguish between the frames, we use the time of capture (in milliseconds from epoch).

void startSavingFrames();

void stopSavingFrames();

std::vector<long long> getSavedFramesTimes();

cv::UMat getSavedFrame(long long frameTime);

void deleteSavedFramesBefore(long long frameTime);

void deleteAllSavedFrames();

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
