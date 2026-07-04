#pragma once
#include "../AnalysisSettings.h"
#include "AnalysisResult.h"
#include "../VideoCaptureManager.h"
#include "../TemplateMatcher.h"
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
namespace IGT {

class TimeRecognizer {
public:
    TimeRecognizer(const AnalysisSettings& settings);

    ~TimeRecognizer();

    /* Recognizes the time on a frame, checks if the frame has a score screen if needed.
     * Returns the found positions of digits and SCORE/TIME labels. */
    std::vector<TemplateMatcher::Match> recognizeTime(cv::UMat frame, bool checkForScoreScreen, bool croppedToGameRect, AnalysisResult& result);

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

    std::chrono::steady_clock::duration getTimeSinceDigitsLocationLastUpdated() const;

    /* Reports the current LiveSplit split index.
     * Should be up-to-date upon calling recognizeTime(). */
    void reportCurrentSplitIndex(int currentSplitIndex);

private:
    std::vector<TemplateMatcher::Match> findLabelsAndUpdateDigitsRect(const cv::UMat& frame, bool croppedToGameRect);

    bool checkRecognizedDigits(std::vector<TemplateMatcher::Match>& digitMatches);

    void getTimeFromRecognizedDigits(const std::vector<TemplateMatcher::Match>& digitMatches, AnalysisResult& result);

    void updateDigitsRect(const std::vector<TemplateMatcher::Match>& labels);

    bool doCheckForScoreScreen(std::vector<TemplateMatcher::Match>& labels, int originalFrameHeight);

    void onRecognitionSuccess();

    void onRecognitionFailure(AnalysisResult& result);

    TemplateMatcher::Match findTopTimeLabel(const std::vector<TemplateMatcher::Match>& labels);

    std::vector<TemplateMatcher::Match> calculateBestScaleAndFindMatches(cv::UMat& frame, const std::string& templateName, bool croppedToGameRect);

    void resetDigitsLocationSync();

    // Crops the frame to the region of interest where the digits are located.
    cv::UMat cropToDigitsRect(const cv::UMat& frame) const;

    /* Increases the contrast for an image. Returns an empty image on error.
     * Needed in order to recognize digits better on a frame before a transition
     * (as the frames before a transition are either too dark or too bright). */
    cv::UMat applyColorCorrection(cv::UMat img) const;

    bool timeIncludesMilliseconds() const;

    static cv::Rect scaleAndRound(cv::Rect src, double scale);

    const AnalysisSettings settings;

    const TemplateMatcher templateMatcher;

    DigitsLocation curDigitsLocation;

    bool recalculatedBestScaleLastTime;

    std::atomic<DigitsLocation> lastSuccessfulDigitsLocation;

    std::chrono::steady_clock::time_point lastRecognitionSuccessTime;

    // Flag for resetDigitsLocationAsync().
    std::atomic<bool> shouldResetDigitsLocation{false};

    // The split index that LiveSplit is currently at.
    int currentSplitIndex = -1;

    class OnSourceChangedListenerImpl : public VideoCaptureManager::OnSourceChangedListener {
    public:
        OnSourceChangedListenerImpl(TimeRecognizer& timeRecognizer);

        void onSourceChanged() override;
    private:
        TimeRecognizer& timeRecognizer;
    } onSourceChangedListener;
};

}  // namespace IGT
}  // namespace SonicVisualSplitBase
