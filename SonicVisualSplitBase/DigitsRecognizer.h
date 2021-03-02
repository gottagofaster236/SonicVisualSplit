#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include <filesystem>

namespace SonicVisualSplitBase {

class DigitsRecognizer {
public:
    static DigitsRecognizer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite);

    // Find locations of all digits, "SCORE" and "TIME" labels.
    std::vector<std::pair<cv::Rect2f, char>> findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen);

    /* We precalculate the rectangle where all of the digits are located.
     * In case of error (e.g. video source properties changed), we may want to recalculate that. */
    static void resetDigitsPlacement();

    // Same as resetDigitsPlacement, but non-blocking.
    static void resetDigitsPlacementAsync();

    bool recalculatedDigitsPlacementLastTime();

    // See bestScale.
    double getBestScale();

    /* There is never more than instance of DigitsRecognizer, so we use a pseudo-singleton pattern to manage the memory easier.
     * This function gets the current instance, or returns nullptr if there's no current instance. */
    static DigitsRecognizer* getCurrentInstance();

    /* Returns the region of interest where the time digits were located last time,
     * with coordinates from 0 to 1 (i.e. relative to the size of the frame).
     * This value is never reset, so that it's possible to estimate 
     * the position of time digits ROI almost all the time. */
    cv::Rect2f getRelativeDigitsRoi();

    // We search for symbols in our code (hack hack).
    static const char SCORE = 'S';
    static const char TIME = 'T';

private:
    DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite);

    std::vector<std::pair<cv::Rect2f, double>> findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement);

    std::vector<std::pair<cv::Rect2f, char>> removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations);

    // Returns the minimum similarity coefficient divided by the best found similarity.
    double getSymbolMinSimilarityCoefficient(char symbol);

    /* Similarity of a symbol may be multiplied by a coefficient
     * in order to make it a less or more preferable option when choosing between symbols. */
    double getSymbolSimilarityMultiplier(char symbol);

    cv::UMat cropToDigitsRoiAndCorrectColor(cv::UMat img);

    /* Finds the highest and lowest pixel values, and corrects the brightness accordingly.
     * Needed in order to recognize digits better on a frame before a transition
     * (as the frames before a transition are either too dark or too bright). */
    void applyColorCorrection(cv::UMat img);

    std::tuple<cv::UMat, cv::UMat, int> loadImageAndMaskFromFile(char symbol);

    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isComposite;

    // Scale of the image which matches the templates (i.e. digits) the best. -1, if not calculated yet.
    double bestScale = -1;

    // The region of interest (ROI) where the time digits are located (i.e. we don't search the whole frame)
    cv::Rect digitsRoi;

    // Similarity coefficient of the best match
    double bestSimilarity;

    bool recalculateDigitsPlacement;

    // map: symbol (a digit, TIME or SCORE) -> {image of the symbol, binary alpha mask, count of opaque pixels}
    std::map<char, std::tuple<cv::UMat, cv::UMat, int>> templates;

    // See getRelativeDigitsRoi().
    cv::Rect2f relativeDigitsRoi;

    inline static DigitsRecognizer* instance = nullptr;
};

}  // namespace SonicVisualSplitBase