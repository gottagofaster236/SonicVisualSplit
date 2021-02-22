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
    static DigitsRecognizer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isRGB);

    // Find locations of all digits, "SCORE" and "TIME" labels.
    std::vector<std::pair<cv::Rect2f, char>> findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen);

    /* We precalculate the rectangle where all of the digits are located.
     * In case of error (e.g. video source properties changed), we may want to recalculate that. */
    static void resetDigitsPlacement();

    // Same as resetDigitsPlacement, but non-blocking.
    static void resetDigitsPlacementAsync();

    bool recalculatedDigitsPlacementLastTime();

    /* There is never more than instance of DigitsRecognizer, so we use a pseudo-singleton pattern to manage the memory easier.
     * This function gets the current instance, or returns nullptr if there's no current instance. */
    static DigitsRecognizer* getCurrentInstance();

    /* Returns the region of interest where the time digits were located last time,
     * with coordinates from 0 to 1 (i.e. relative to the size of the frame).
     * This value is never reset, so that it's possible to estimate 
     * the position of time digits ROI almost all the time. */
    cv::Rect2f getRelativeDigitsRoi();

    // We search for symbols in our code
    static const char SCORE = 'S';
    static const char TIME = 'T';

private:
    DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isRGB);

    std::vector<std::pair<cv::Rect2f, double>> findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement);

    std::vector<std::pair<cv::Rect2f, char>> removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations);

    std::tuple<cv::UMat, cv::UMat, int> loadImageAndMaskFromFile(char symbol);

    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isRGB;

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

    // CONSTANTS
    // Minimum similarity of a match (zero is a perfect match)
    static constexpr double MIN_SIMILARITY = -10000;

    // Minimum similarity in relation to the best found similarity
    static constexpr double SIMILARITY_COEFFICIENT = 3.5;

    /* Minimum similarity of "TIME" in relation to the best found similarity.
     * (We use "TIME" to detect the score screen, so we want to be sure). */ 
    static constexpr double TIME_SIMILARITY_COEFFICIENT = 2.5;
};

}  // namespace SonicVisualSplitBase