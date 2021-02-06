#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace SonicVisualSplitBase {
namespace FrameStorage {

// This module saves frames from OBS for further usage (as we can't process all of them in real-time).
// To distinguish between the frames, we use the time of capture (in milliseconds from epoch).

void startSavingFrames();

void stopSavingFrames();

std::vector<long long> getSavedFramesTimes();

cv::UMat getSavedFrame(long long frameTime);

// Deletes saved frames with time of save less than frameTime
void deleteSavedFramesBefore(long long frameTime);

void deleteAllSavedFrames();

// Sometimes we may recognize the time on a frame incorrectly. If we detect that, we mark that frame as such.
void markFrameAsRecognizedIncorrectly(long long frameTime);

bool isFrameRecognizedIncorrectly(long long frameTime);

}  // namespace SonicVisualSplitBase
}  // namespace FrameStorage
