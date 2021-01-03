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

void startSavingFrames();

std::vector<long long> getSavedFramesTimes();

std::vector<cv::UMat> getSavedFrame(long long frameTime);

}
