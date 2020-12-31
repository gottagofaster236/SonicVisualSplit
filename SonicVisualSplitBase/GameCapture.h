#pragma once
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace SonicVisualSplitBase {

// Gets the stream preview from the opened OBS window
cv::UMat getGameFrame();

// void saveFrameToBuffer(); IDK

} // namespace SonicVisualSplitBase