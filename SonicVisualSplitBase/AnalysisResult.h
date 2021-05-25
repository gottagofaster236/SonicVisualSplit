#pragma once
#include <string>
#include <opencv2/core.hpp>
#undef NO_ERROR


namespace SonicVisualSplitBase {

enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

struct AnalysisResult {
    bool recognizedTime = false;
    int timeInMilliseconds = 0;  // The time on the screen converted to milliseconds.
    std::string timeString;  // The time on the screen, e.g. 1'23"45.
    bool isScoreScreen = false;  // Needed to understand if we finished the level.
    bool isBlackScreen = false;  // E.g. a transition screen between levels.
    bool isWhiteScreen = false;  // E.g. a transition screen to a special stage.
    cv::Mat visualizedFrame;
    long long frameTime = 0;
    ErrorReasonEnum errorReason = ErrorReasonEnum::NO_ERROR;
};

}