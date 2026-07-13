#pragma once
#include <string>
#include <opencv2/core.hpp>
#undef NO_ERROR


namespace SonicVisualSplitBase {
namespace RTA {

// TODO: rework this for RTA-only case
struct AnalysisResult {
    long long frameTime = 0;
    int timeBonusPoints = 0;
};

}  // namespace RTA
}  // namespace SonicVisualSplitBase
