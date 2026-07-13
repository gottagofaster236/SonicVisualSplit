#pragma once
#include "TimeRecognizer.h"
#include "../AnalysisSettings.h"
#include "AnalysisResult.h"
#include "FrameStorage.h"
#include <opencv2/core.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <optional>


namespace SonicVisualSplitBase {
namespace IGT {

// This class finds the digits on the game frame (and gets a few other parameters, see AnalysisResult).
class FrameAnalyzer {
public:
    FrameAnalyzer(const AnalysisSettings& settings);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize, const std::optional<cv::Rect>& gameRect);

    // See TimeRecognizer::resetDigitsLocation.
    void resetDigitsLocation();

    /* Reports the current LiveSplit split index.
     * Should be up-to-date upon calling analyzeFrame(). */
    void reportCurrentSplitIndex(int currentSplitIndex);

    bool checkForResetScreen();

    FrameStorage frameStorage;

private:
    cv::UMat fixAspectRatio(const cv::UMat& frame) const;

    double getAspectRatioScaleFactor() const;

    /* Returns several areas of the template which have to be matched
     * to detect a reset screen.
     * We don't simply compare the whole template, for example
     * because then Sonic 2's EH2 would match the current template. */
    std::vector<cv::Rect> getResetTemplateMatchAreas();

    cv::Rect getResetTemplateSearchArea(cv::Rect resetTemplateArea);

    static cv::UMat matchResetTemplates(cv::UMat image, cv::UMat templ);

    bool checkIfFrameIsSingleColor(cv::UMat frame, bool hasGameRect);

    bool checkIfImageIsSingleColor(cv::UMat img, cv::Scalar color, double maxAvgDifference);

    void visualizeResult(const std::vector<TemplateMatcher::Match>& allMatches, cv::UMat originalFrame, const std::optional<cv::Rect>& gameRect);

    const AnalysisSettings settings;

    TimeRecognizer timeRecognizer;

    cv::UMat resetTemplate;

    // Temporary field for the functions.
    AnalysisResult result;
};

}  // namespace IGT
}  // namespace SonicVisualSplitBase
