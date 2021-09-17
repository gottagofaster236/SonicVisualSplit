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

class FrameAnalyzer;

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
    void resetDigitsPositions();

    /* Returns the scale of the image which matches the templates (i.e. digits) the best,
     * or -1, if not calculated yet. This method is thread-safe. */
    double getBestScale() const;
    
    /* Returns the time rect for the frame size. Returns an empty rectangle if not calculated yet.
     * This method is thread-safe. */
    cv::Rect getTimeRectForFrameSize(cv::Size frameSize);

    std::chrono::steady_clock::duration getTimeSinceDigitsPositionsLastUpdated();

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

    void resetDigitsPlacementSync();

    // Returns the global minimum acceptable similarity of a symbol.
    double getGlobalMinSimilarity(char symbol) const;

    /* Returns the minimum acceptable similarity in relation to the best found similarity.
     * Without parameters, returns the default value. */
    double getMinSimilarityDividedByBestSimilarity(char symbol = 0) const;

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

    // Scale of the image which matches the templates (i.e. digits) the best. -1, if not calculated yet.
    std::atomic<double> bestScale{-1};

    bool recalculatedBestScaleLastTime;

    /* The rectangle where the time digits are located (i.e. we don't search the whole frame).
     * (This rectangle is valid after the frame has been scaled down to bestScale). */
    cv::Rect digitsRect;

    // Needed in case if score screen check fails.
    cv::Rect prevDigitsRect;

    /* cv::Rect2f is not trivially copyable, and thus 
     * it's not possible to create std::atomic<cv::Rect2f>. */
    struct Rect2fPOD {  
        float x, y, width, height;

        Rect2fPOD() : x(0), y(0), width(0), height(0) {}

        Rect2fPOD(cv::Rect2f rect) : x(rect.x), y(rect.y), width(rect.width), height(rect.height) {}

        operator cv::Rect2f() { return {x, y, width, height}; }
    };

    /* The rectangle where the time digits were located last time,
     * with coordinates from 0 to 1 (i.e. relative to the size of the frame).
     * Unlike digitsRect, it's never reset, so that it's possible to estimate
     * the position of time digits ROI almost all the time. */
    std::atomic<Rect2fPOD> relativeTimeRect{Rect2fPOD()};

    /* The bounding rectangle of the "TIME" label on the original frame. */
    cv::Rect2f timeRect;

    std::chrono::steady_clock::time_point relativeTimeRectUpdatedTime;

    cv::Size lastFrameSize;

    // Map: symbol (a digit, TIME or SCORE) -> {image of the symbol, binary alpha mask, count of opaque pixels}.
    std::map<char, std::tuple<cv::UMat, cv::UMat, int>> templates;

    // Flag for resetDigitsPlacementAsync().
    std::atomic<bool> shouldResetDigitsPlacement{false};

    // The split index that LiveSplit is currently at.
    int currentSplitIndex = -1;

    class OnSourceChangedListenerImpl : public FrameStorage::OnSourceChangedListener {
    public:
        OnSourceChangedListenerImpl(TimeRecognizer& timeRecognizer);

        void onSourceChanged() const override;
    private:
        TimeRecognizer& timeRecognizer;
    };
    OnSourceChangedListenerImpl onSourceChangedListener;
};

}  // namespace SonicVisualSplitBase