#pragma once
#include "AnalysisSettings.h"
#include <vector>
#include <string>
#include <map>
#include <opencv2/core.hpp>

namespace SonicVisualSplitBase {

class TemplateMatcher {
public:
    TemplateMatcher(const AnalysisSettings& settings, const std::vector<std::string>& templateNames);

    ~TemplateMatcher();

    struct Match {
        cv::Rect location;
        std::string templateName;
        double similarity;

        bool operator==(const Match& other) const;
    };

    // Note: this already calls convertToGray and sortAndRemoveOverlappingMatches
    std::vector<Match> findTemplateLocations(const cv::UMat& src, const std::vector<std::string>& templateNames, bool allMatchesHaveSameYCoord) const;

    static void sortAndRemoveOverlappingMatches(std::vector<Match>& matches, bool allMatchesHaveSameYCoord);

    /* Converts a frame to grayscale.
     * (This method is needed to speed up template matching by reducing the number of channels to 1). */
    static cv::UMat convertToGray(const cv::UMat& src, bool filterYellowColor);

    // Template names for "SCORE" and "TIME"
    static const std::string SCORE;
    static const std::string TIME;
    static const std::vector<std::string> DIGIT_TEMPLATES;
    static const std::vector<std::string> ALL_TEMPLATES;

private:
    std::vector<Match> findTemplateLocations(const cv::UMat& src, const std::string& templateName, bool allMatchesHaveSameYCoord) const;

    void removeMatchesWithIncorrectYCoord(std::vector<Match>& digitMatches) const;

    // Returns the minimum acceptable similarity of a template.
    double getMinSimilarity(const std::string& templateName) const;

    /* Similarity of a template may be multiplied by a coefficient
     * in order to make it a less or more preferable option when choosing between symbols. */
    double getSimilarityMultiplier(const std::string& templateName) const;

    struct Template {
        cv::UMat image;
        cv::UMat mask;
        int countOpaquePixels;
    };
 
    Template loadTemplate(const std::string& templateName);

    bool shouldFilterYellowColor(const std::string& templateName) const;

    cv::UMat getAlphaMask(const cv::UMat& image) const;

    const AnalysisSettings& settings;
    std::map<std::string, Template> templates;
};

}
