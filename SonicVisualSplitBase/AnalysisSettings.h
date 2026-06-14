#pragma once
#include <string>
#include <filesystem>
#include <opencv2/core.hpp>


namespace SonicVisualSplitBase {

enum class Game {
    Sonic1,
    Sonic2,
    SonicCD,
};

struct AnalysisSettings {
    Game game;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    bool isComposite;
    bool autoResetEnabled;

    cv::UMat loadTemplateImageFromFile(char symbol) const;

    cv::UMat loadTemplateImageFromFile(std::string filename) const;
};

}
