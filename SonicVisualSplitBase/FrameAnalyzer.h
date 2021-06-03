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
    static FrameAnalyzer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
        bool isStretchedTo16By9, bool isComposite);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize);

    // Must be called before calling analyzeFrame.
    static void reportCurrentSplitIndex(int currentSplitIndex);

    static int getCurrentSplitIndex();

    // This header is included by C++/CLI, which doesn't have <mutex>.
    static void lockFrameAnalysisMutex();

    static void unlockFrameAnalysisMutex();

    static void saveLastFailedFrame();

    FrameAnalyzer(FrameAnalyzer& other) = delete;
    void operator=(const FrameAnalyzer&) = delete;

private:
    FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9, bool isComposite);

    cv::UMat getSavedFrame(long long frameTime);

    bool checkIfFrameIsSingleColor(cv::UMat frame);

    bool checkIfImageIsSingleColor(cv::UMat img, cv::Scalar color, double maxAvgDifference);

    void visualizeResult(const std::vector<TimeRecognizer::Match>& allMatches);

    inline static std::unique_ptr<FrameAnalyzer> instance;

    // Settings.
    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    bool isComposite;

    // Temporary fields for the functions.
    AnalysisResult result;
    cv::UMat originalFrame;

    inline static int currentSplitIndex;

    inline static cv::UMat lastFailedFrame;
};

}  // namespace SonicVisualSplitBase