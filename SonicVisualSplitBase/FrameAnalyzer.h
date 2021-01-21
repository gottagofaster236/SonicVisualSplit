#pragma once
#include <vector>
#include <filesystem>
#ifdef __cplusplus_cli
#pragma managed(push, off)
#endif
#include <opencv2/opencv.hpp>
#ifdef __cplusplus_cli
#pragma managed(pop)
#endif

namespace SonicVisualSplitBase {

enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

struct AnalysisResult {
    bool foundAnyDigits = false;
    std::string timeDigits;  // e. g. if the time is 1'08"23 then the string will be "10823"
    bool isScoreScreen = false;  // needed to understand if we finished the level
    bool isBlackScreen = false;  // e.g. transition screen
    cv::Mat visualizedFrame;
    ErrorReasonEnum errorReason = ErrorReasonEnum::NO_ERROR;
};


// This class finds the digits on the game frame (and gets a few other parameters, see AnalysisResult).
class FrameAnalyzer {
public:
    static FrameAnalyzer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize, bool recalculateOnError);

private:
    FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9);

    void checkRecognizedSymbols(const std::vector<std::pair<cv::Rect2f, char>>& allSymbols, cv::UMat originalFrame, bool checkForScoreScreen, bool visualize);

    void visualizeResult(const std::vector<std::pair<cv::Rect2f, char>>& symbols);

    static bool checkIfFrameIsBlack(cv::UMat frame);

    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    AnalysisResult result;

    inline static FrameAnalyzer* instance = nullptr;
};

}  // namespace SonicVisualSplitBase