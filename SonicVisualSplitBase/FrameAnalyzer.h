#pragma once
#include <opencv2/core.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <memory>
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
    bool isBlackScreen = false;  // e.g. transition screen between levels
    bool isWhiteScreen = false;  // e.g. transition screen to a special stage
    cv::Mat visualizedFrame;
    long long frameTime;
    ErrorReasonEnum errorReason = ErrorReasonEnum::NO_ERROR;
};


// This class finds the digits on the game frame (and gets a few other parameters, see AnalysisResult).
class FrameAnalyzer {
public:
    static FrameAnalyzer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
        bool isStretchedTo16By9, bool isComposite);

    AnalysisResult analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize);

    // This header is included by C++/CLI, which doesn't have <mutex>.
    static void lockFrameAnalyzationMutex();

    static void unlockFrameAnalyzationMutex();

private:
    FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9, bool isComposite);

    void checkRecognizedSymbols(bool checkForScoreScreen, bool visualize);

    void doCheckForScoreScreen(std::map<char, std::vector<cv::Rect2f>>& scoreAndTimePositions);

    void visualizeResult();

    enum class SingleColor { BLACK, WHITE, NOT_SINGLE_COLOR };
    SingleColor checkIfFrameIsSingleColor(cv::UMat frame);

    // Settings.
    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    bool isComposite;

    // Temporary fields for the functions.
    AnalysisResult result;
    std::vector<std::pair<cv::Rect2f, char>> recognizedSymbols;
    cv::UMat originalFrame;

    inline static std::unique_ptr<FrameAnalyzer> instance;
};

}  // namespace SonicVisualSplitBase