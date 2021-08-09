#pragma once
#include <string>
#include <filesystem>
#include <opencv2/core.hpp>


namespace SonicVisualSplitBase {

struct AnalysisSettings {
    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isStretchedTo16By9;
    bool isComposite;

    cv::UMat loadTemplateImageFromFile(char symbol) const;
};

}