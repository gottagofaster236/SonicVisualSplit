#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include <filesystem>
#include <memory>
#include <atomic>
#include <chrono>


namespace SonicVisualSplitBase {

class DigitsRecognizer {
public:
    static DigitsRecognizer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite);

    struct Match {
        cv::Rect2f location;
        char symbol;
        double similarity;

        bool operator==(const Match& other) const;
    };

    // Finds locations of all digits, "SCORE" and "TIME" labels.
    std::vector<Match> findLabelsAndDigits(cv::UMat frame, bool checkForScoreScreen);

    /* We precalculate the rectangle where all of the digits are located.
     * In case of error (e.g. video source properties changed), we may want to recalculate that. */
    static void resetDigitsPlacement();

    // Same as resetDigitsPlacement, but non-blocking.
    static void resetDigitsPlacementAsync();

    // Resetting everything we precalculated.
    static void fullReset();

    // Returns the scale of the image which matches the templates (i.e. digits) the best, or -1, if not calculated yet..
    double getBestScale() const;

    // Called from FrameAnalyzer after the frame is checked.
    void reportRecognitionSuccess();

    void reportRecognitionFailure();

    /* Returns the rectangle where the time digits were located last time,
     * with coordinates from 0 to 1 (i.e. relative to the size of the frame).
     * Unlike digitsRect, it's never reset, so that it's possible to estimate
     * the position of time digits ROI almost all the time. */
    cv::Rect2f getRelativeDigitsRect();

    /* There is never more than instance of DigitsRecognizer.
     * This function gets the current instance, or returns nullptr if there's no current instance. */
    static const std::unique_ptr<DigitsRecognizer>& getCurrentInstance();

    DigitsRecognizer(DigitsRecognizer& other) = delete;
    void operator=(const DigitsRecognizer&) = delete;

    static const int MAX_ACCEPTABLE_FRAME_HEIGHT = 640;

    // We search for symbols in our code (hack hack).
    static const char SCORE = 'S';
    static const char TIME = 'T';

private:
    DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite);

    std::vector<Match> findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement);

    void removeMatchesWithLowSimilarity(std::vector<Match>& matches);

    void removeOverlappingMatches(std::vector<Match>& matches);

    void removeMatchesWithIncorrectYCoord(std::vector<Match>& digitMatches);

    /* Returns the minimum similarity coefficient divided by the best found similarity.
     * Without parameters, returns the default similarity. */
    double getSymbolMinSimilarityCoefficient(char symbol = 0);

    /* Similarity of a symbol may be multiplied by a coefficient
     * in order to make it a less or more preferable option when choosing between symbols. */
    double getSymbolSimilarityMultiplier(char symbol);

    /* Crops the frame to the region of interest where the digits are located,
     * and increases the contrast for the resulting image. Returns an empty image on error. */
    cv::UMat cropToDigitsRectAndCorrectColor(cv::UMat frame);

    /* Increases the contrast for an image. Returns an empty image on error.
     * Needed in order to recognize digits better on a frame before a transition
     * (as the frames before a transition are either too dark or too bright). */
    cv::UMat applyColorCorrection(cv::UMat img);

    /* Converts a frame to grayscale.
     * If filterYellowColor is true, then yellow will be white in the resulting image.
     * (This method is needed to speed up template matching by reducing the number of channels to 1). */
    static cv::UMat convertFrameToGray(cv::UMat frame, bool filterYellowColor = false);

    /* Loads a template image from file, separates its alpha channel.
     * Returns a tuple of {image, binary alpha mask, count of opaque pixels}. */
    std::tuple<cv::UMat, cv::UMat, int> loadImageAndMaskFromFile(char symbol);
    
    // Settings.
    std::string gameName;
    std::filesystem::path templatesDirectory;
    bool isComposite;

    // Scale of the image which matches the templates (i.e. digits) the best. -1, if not calculated yet.
    double bestScale = -1;

    /* The rectangle where the time digits are located (i.e. we don't search the whole frame).
     * (This rectangle is valid after the frame has been scaled down to bestScale). */
    cv::Rect digitsRect;

    bool recalculatedDigitsPlacementLastTime;

    // See getRelativeDigitsRect().
    cv::Rect2f relativeDigitsRect;

    std::chrono::system_clock::time_point relativeDigitsRectUpdatedTime;

    cv::Size lastFrameSize;

    // Map: symbol (a digit, TIME or SCORE) -> {image of the symbol, binary alpha mask, count of opaque pixels}.
    std::map<char, std::tuple<cv::UMat, cv::UMat, int>> templates;

    // Flag for resetDigitsPlacementAsync().
    inline static std::atomic<bool> shouldResetDigitsPlacement;

    inline static std::unique_ptr<DigitsRecognizer> instance;
};

}  // namespace SonicVisualSplitBase