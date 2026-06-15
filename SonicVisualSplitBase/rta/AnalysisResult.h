#pragma once
#include <string>
#include <opencv2/core.hpp>
#undef NO_ERROR


namespace SonicVisualSplitBase {
namespace RTA {

enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_GAME_RECT, UNSUPPORTED_GAME, NO_ERROR,
};

struct AnalysisResult {
    cv::Mat visualizedFrame;
    ErrorReasonEnum errorReason = ErrorReasonEnum::NO_ERROR;
};

}  // namespace RTA
}  // namespace SonicVisualSplitBase
