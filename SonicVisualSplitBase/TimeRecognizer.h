#pragma once
#include "AnalysisSettings.h"
#include "AnalysisResult.h"
#include "FrameStorage.h"
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

class TimeRecognizer {
public:
    TimeRecognizer(const AnalysisSettings& settings);

    ~TimeRecognizer();

    struct Match {
        cv::Rect2f location;
        char symbol;
        double similarity;

        bool operator==(const Match& other) const;
    };

    /* Recognizes the time on a frame, checks if the frame has a score screen if needed.
     * Returns the found positions of digits and SCORE/TIME labels. */
    std::vector<Match> recognizeTime(cv::UMat frame, bool checkForScoreScreen, AnalysisResult& result);

    /* We precalculate the rectangle where all of the digits are located.
     * In case of error (e.g. video source properties changed), we may want to recalculate that. */
    void resetDigitsLocation();

    struct DigitsLocation {
        /* Scale of the image which matches the templates (i.e.digits) the best.
         * -1, if not calculated yet. */
        double bestScale = -1;

        /* The rectangle where the time digits are located
         * after the frame has been scaled down to bestScale. */
        cv::Rect digitsRect;

        /* The bounding rectangle of the "TIME" label
         * after the frame has been scaled down to bestScale. */
        cv::Rect timeRect;

        // The size of the frame for which the above properties hold true.
        cv::Size frameSize;

        bool isValid() const;
    };

    // This method is thread-safe.
    DigitsLocation getLastSuccessfulDigitsLocation();

    std::chrono::steady_clock::duration getTimeSinceDigitsLocationLastUpdated();

    /* Reports the current LiveSplit split index.
     * Should be up-to-date upon calling recognizeTime(). */
    void reportCurrentSplitIndex(int currentSplitIndex);

    static const int MAX_ACCEPTABLE_FRAME_HEIGHT = 640;

    // We search for symbols in our code (hack hack).
    static const char SCORE = 'S';
    static const char TIME = 'T';

private:
    std::vector<Match> findLabelsAndUpdateDigitsRect(cv::UMat frame);

    bool checkRecognizedDigits(std::vector<Match>& digitMatches);

    void getTimeFromRecognizedDigits(const std::vector<Match>& digitMatches, AnalysisResult& result);

    void updateDigitsRect(const std::vector<Match>& labels);

    bool doCheckForScoreScreen(std::vector<Match>& labels, int originalFrameHeight);

    void onRecognitionSuccess();

    void onRecognitionFailure(AnalysisResult& result);

    Match findTopTimeLabel(const std::vector<Match>& labels);
    
    std::vector<Match> findSymbolLocations(cv::UMat frame, char symbol, bool recalculateBestScale);

    void removeMatchesWithLowSimilarity(std::vector<Match>& matches);

    void removeOverlappingMatches(std::vector<Match>& matches);

    void removeMatchesWithIncorrectYCoord(std::vector<Match>& digitMatches);

    void resetDigitsLocationSync();

    // Returns the minimum acceptable similarity of a symbol.
    double getMinSimilarity(char symbol) const;

    /* Similarity of a symbol may be multiplied by a coefficient
     * in order to make it a less or more preferable option when choosing between symbols. */
    double getSimilarityMultiplier(char symbol) const;

    // Crops the frame to the region of interest where the digits are located.
    cv::UMat cropToDigitsRect(cv::UMat frame);

    /* Increases the contrast for an image. Returns an empty image on error.
     * Needed in order to recognize digits better on a frame before a transition
     * (as the frames before a transition are either too dark or too bright). */
    cv::UMat applyColorCorrection(cv::UMat img);

    /* Converts a frame to grayscale.
     * If filterYellowColor is true, then yellow will be white in the resulting image.
     * (This method is needed to speed up template matching by reducing the number of channels to 1). */
    static cv::UMat convertFrameToGray(cv::UMat frame, bool filterYellowColor = false);

    bool timeIncludesMilliseconds() const;

    void loadAllTemplates();

    // Returns a tuple of {gray image, binary alpha mask, count of opaque pixels}.
    std::tuple<cv::UMat, cv::UMat, int> loadSymbolTemplate(char symbol);

    cv::UMat getAlphaMask(cv::UMat image);
    
    const AnalysisSettings settings;

    DigitsLocation curDigitsLocation;

    bool recalculatedBestScaleLastTime;

    std::atomic<DigitsLocation> lastSuccessfulDigitsLocation;

    std::chrono::steady_clock::time_point lastRecognitionSuccessTime;

    // Map: symbol (a digit, TIME or SCORE) -> {image of the symbol, binary alpha mask, count of opaque pixels}.
    std::map<char, std::tuple<cv::UMat, cv::UMat, int>> templates;

    // Flag for resetDigitsLocationAsync().
    std::atomic<bool> shouldResetDigitsLocation{false};

    // The split index that LiveSplit is currently at.
    int currentSplitIndex = -1;

    class OnSourceChangedListenerImpl : public FrameStorage::OnSourceChangedListener {
    public:
        OnSourceChangedListenerImpl(TimeRecognizer& timeRecognizer);

        void onSourceChanged() const override;
    private:
        TimeRecognizer& timeRecognizer;
    } onSourceChangedListener;
};

}  // namespace SonicVisualSplitBase