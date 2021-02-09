#pragma once
#include <opencv2/core.hpp>
#include <filesystem>
#include <vector>
#include <string>
#undef NO_ERROR


namespace SonicVisualSplitBase {

enum class ErrorReasonEnum {
    VIDEO_DISCONNECTED, NO_TIME_ON_SCREEN, NO_ERROR
};

struct AnalysisResult {
    bool recognizedTime = false;
    int timeInMilliseconds = 0;  // the time on the screen converted to milliseconds
    std::string timeString;  // the time on the screen, e. g. 1'08"23
    bool isScoreScreen = false;  // needed to understand if we finished the level
    bool isBlackScreen = false;  // e.g. transition screen
    cv::Mat visualizedFrame;
    long long frameTime;
    ErrorReasonEnum errorReason = ErrorReasonEnum::NO_ERROR;
};


// This class finds the digits on the game frame (and gets a few other parameters, see AnalysisResult).
class FrameAnalyzer {
public:
    static FrameAnalyzer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize, bool recalculateOnError);

    // We precalculate the rectangle where all of the digits are located.
    // In case of error (e.g. video source properties changed), we may want to recalculate that.
    static void resetDigitsPlacement();

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