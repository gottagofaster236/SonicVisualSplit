#include "AnalysisSettings.h"
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <iterator>


namespace SonicVisualSplitBase {

cv::UMat AnalysisSettings::loadTemplateImageFromFile(char symbol) const {
    // The path can contain unicode, so we read the file by ourselves.
    std::filesystem::path templatePath = templatesDirectory / (symbol + std::string(".png"));
    std::ifstream fin(templatePath, std::ios::binary);
    std::vector<uint8_t> fileBuffer((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    fin.close();

    cv::UMat templateImage;
    cv::imdecode(fileBuffer, cv::IMREAD_UNCHANGED).copyTo(templateImage);
    return templateImage;
}

}