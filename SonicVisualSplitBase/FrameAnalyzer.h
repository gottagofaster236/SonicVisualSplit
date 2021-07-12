#pragma once
#include "TimeRecognizer.h"
#include "AnalysisResult.h"
#include <opencv2/core.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <memory>


namespace SonicVisualSplitBase {

// This class finds the digits on the game frame (and gets a few other parameters, see AnalysisResult).
class FrameAnalyzer {
public:
    FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9, bool isComposite);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize);

    // Calls resetDigitsPlacement() for the inner TimeRecognizer.
    void resetDigitsPlacement();

    // Must be called before calling analyzeFrame.
    void reportCurrentSplitIndex(int currentSplitIndex);

    int getCurrentSplitIndex();

    bool checkForResetScreen(long long frameTime);

    // This header is included by C++/CLI, which doesn't have <mutex>.
    void lockFrameAnalysisMutex();

    void unlockFrameAnalysisMutex();

private:
    cv::UMat getSavedFrame(long long frameTime);

    bool checkIfFrameIsSingleColor(cv::UMat frame);

    bool checkIfImageIsSingleColor(cv::UMat img, cv::Scalar color, double maxAvgDifference);

    void visualizeResult(const std::vector<TimeRecognizer::Match>& allMatches);

    // Settings.
    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    bool isComposite;

    TimeRecognizer timeRecognizer;

    // Temporary fields for the functions.
    AnalysisResult result;
    cv::UMat originalFrame;

    int currentSplitIndex;
};

}  // namespace SonicVisualSplitBase