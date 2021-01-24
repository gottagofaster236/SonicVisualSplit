#pragma once
#include <vector>
#include <string>
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {
namespace GameCapture {

// returns the full screenshot of the OBS window
cv::Mat getObsScreenshot();

// returns the stream preview
cv::UMat getGameFrameFromObsScreenshot(cv::Mat screenshot);

// returns the minimum acceptable height of the OBS window (so that the stream preview isn't too small)
int getMinimumObsHeight();

}  // namespace GameCapture
}  // namespace SonicVisualSplitBase