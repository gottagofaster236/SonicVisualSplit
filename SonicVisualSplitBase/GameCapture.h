#pragma once
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace SonicVisualSplitBase {
namespace GameCapture {

// returns the full screenshot of the OBS window
cv::Mat getObsScreenshot();

// returns the stream preview
cv::UMat getGameFrameFromObsScreenshot(cv::Mat screenshot);

}  // namespace GameCapture
}  // namespace SonicVisualSplitBase