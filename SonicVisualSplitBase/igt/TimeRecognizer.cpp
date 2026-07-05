#include "TimeRecognizer.h"
#include "FrameAnalyzer.h"
#include "FrameStorage.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <ranges>
#include <algorithm>
#include <utility>
#include <cctype>
#include <cmath>
#include <format>


namespace SonicVisualSplitBase {
namespace IGT {


TimeRecognizer::TimeRecognizer(const AnalysisSettings& settings) :
    settings(settings), 
    templateMatcher(settings, TemplateMatcher::ALL_TEMPLATES),
    onSourceChangedListener(*this) {
    VideoCaptureManager::addOnSourceChangedListener(onSourceChangedListener);
}


TimeRecognizer::~TimeRecognizer() {
    VideoCaptureManager::removeOnSourceChangedListener(onSourceChangedListener);
}


std::vector<TemplateMatcher::Match> TimeRecognizer::recognizeTime
(cv::UMat frame, bool checkForScoreScreen, bool croppedToGameRect, AnalysisResult& result) {
    if (shouldResetDigitsLocation) {
        resetDigitsLocationSync();
        shouldResetDigitsLocation = false;
    }

    if (frame.size() != curDigitsLocation.frameSize) {
        resetDigitsLocationSync();
        curDigitsLocation.frameSize = frame.size();
    }

    std::vector<TemplateMatcher::Match> allMatches;

    if (curDigitsLocation.isValid()) {
        cv::resize(frame, frame, {}, curDigitsLocation.bestScale, curDigitsLocation.bestScale, cv::INTER_AREA);
        recalculatedBestScaleLastTime = false;
    }
    else {
        // When we're looking for the labelMatches, we're recalculating the best scale (if needed) and the digitsRect.
        checkForScoreScreen = true;
    }
    if (checkForScoreScreen) {
        std::vector<TemplateMatcher::Match> labels = findLabelsAndUpdateDigitsRect(frame, croppedToGameRect);
        if (curDigitsLocation.digitsRect.empty())
            curDigitsLocation.digitsRect = lastSuccessfulDigitsLocation.load().digitsRect;
        if (!curDigitsLocation.isValid()) {
            onRecognitionFailure(result);
            return allMatches;
        }
        if (recalculatedBestScaleLastTime)
            cv::resize(frame, frame, {}, curDigitsLocation.bestScale, curDigitsLocation.bestScale);
        result.isScoreScreen = doCheckForScoreScreen(labels, frame.rows);
        allMatches.insert(allMatches.end(), labels.begin(), labels.end());
    }
    frame = cropToDigitsRect(frame);
    frame = applyColorCorrection(frame);
    if (frame.empty()) {
        onRecognitionFailure(result);
        return allMatches;
    }

    std::vector<TemplateMatcher::Match> digitMatches = templateMatcher.findTemplateLocations(frame, TemplateMatcher::DIGIT_TEMPLATES, true, currentSplitIndex);

    if (checkRecognizedDigits(digitMatches))
        getTimeFromRecognizedDigits(digitMatches, result);

    if (result.recognizedTime)
        onRecognitionSuccess();
    else
        onRecognitionFailure(result);

    for (TemplateMatcher::Match& match : digitMatches) {
        // The frame is cropped to digitsRect to speed up the search. Now we have to compensate for that.
        match.location += cv::Point(curDigitsLocation.digitsRect.x, curDigitsLocation.digitsRect.y);
    }

    allMatches.insert(allMatches.end(), digitMatches.begin(), digitMatches.end());
    double bestScale = curDigitsLocation.bestScale;
    if (bestScale == -1) bestScale = 1;
    for (TemplateMatcher::Match& match : allMatches) {
        match.location.x = static_cast<int>(std::round(match.location.x / bestScale));
        match.location.y = static_cast<int>(std::round(match.location.y / bestScale));
        match.location.width = static_cast<int>(std::round(match.location.width / bestScale));
        match.location.height = static_cast<int>(std::round(match.location.height / bestScale));
    }
    return allMatches;
}


void TimeRecognizer::resetDigitsLocation() {
    // Digits placement will be reset on the next call to recognizeTime.
    shouldResetDigitsLocation = true;
}

bool TimeRecognizer::DigitsLocation::isValid() const {
    return bestScale != -1 && !digitsRect.empty();
}


void TimeRecognizer::resetDigitsLocationSync() {
    curDigitsLocation = DigitsLocation();
}


TimeRecognizer::OnSourceChangedListenerImpl::OnSourceChangedListenerImpl(TimeRecognizer& timeRecognizer)
    : timeRecognizer(timeRecognizer) {}


void TimeRecognizer::OnSourceChangedListenerImpl::onSourceChanged() {
    timeRecognizer.resetDigitsLocation();
}


TimeRecognizer::DigitsLocation TimeRecognizer::getLastSuccessfulDigitsLocation(){
    return lastSuccessfulDigitsLocation;
}


std::chrono::steady_clock::duration TimeRecognizer::getTimeSinceDigitsLocationLastUpdated() const {
    return std::chrono::steady_clock::now() - lastRecognitionSuccessTime;
}


void TimeRecognizer::reportCurrentSplitIndex(int currentSplitIndex) {
    this->currentSplitIndex = currentSplitIndex;
}


std::vector<TemplateMatcher::Match> TimeRecognizer::findLabelsAndUpdateDigitsRect(const cv::UMat& frame, bool croppedToGameRect) {
    // Template matching for TIME and SCORE is done on the grayscale frame where yellow is white (it's more accurate this way).
    cv::UMat frameWithYellowFilter = TemplateMatcher::convertToGray(frame, true);
    bool recalculateBestScale = (curDigitsLocation.bestScale == -1);
    recalculatedBestScaleLastTime = recalculateBestScale;

    std::vector<TemplateMatcher::Match> timeMatches;
    if (recalculateBestScale) {
        timeMatches = calculateBestScaleAndFindMatches(frameWithYellowFilter, TemplateMatcher::TIME, croppedToGameRect);
    } else {
        timeMatches = templateMatcher.findTemplateLocations(frameWithYellowFilter, std::vector<std::string>{TemplateMatcher::TIME}, false);
    }
    if (timeMatches.empty()) return {};

    // Checking only the top half of the frame, so that "SCORE" is searched faster.
    cv::Rect topHalf = {0, 0, frameWithYellowFilter.cols, frameWithYellowFilter.rows / 2};
    frameWithYellowFilter = frameWithYellowFilter(topHalf);
    std::vector<TemplateMatcher::Match> scoreMatches = templateMatcher.findTemplateLocations(frameWithYellowFilter, std::vector<std::string>{TemplateMatcher::SCORE}, false);
    if (scoreMatches.empty())
        return {};

    std::vector<TemplateMatcher::Match> labelMatches = std::move(timeMatches);
    labelMatches.insert(labelMatches.end(), scoreMatches.begin(), scoreMatches.end());
    TemplateMatcher::sortAndRemoveOverlappingMatches(labelMatches, false);
    if (labelMatches.size() > 15) {
        // There can't be that many matches, even if some of them are wrong.
        return {};
    }

    /* We've searched for "TIME" and "SCORE". (Searching "SCORE" so that it's not confused with "TIME".)
     * We'll look for digits only in the rectangle to the right of "TIME".
     * That's why we'll crop the frame to that rectangle. */
    updateDigitsRect(labelMatches);
    return labelMatches;
}


bool TimeRecognizer::checkRecognizedDigits(std::vector<TemplateMatcher::Match>& digitMatches) {
    std::ranges::sort(digitMatches, {}, [](const TemplateMatcher::Match& match) {
        return match.location.x;
    });

    for (int i = 0; i + 1 < digitMatches.size(); i++) {
        const cv::Rect& prevLocation = digitMatches[i].location, nextLocation = digitMatches[i + 1].location;
        int interval = (nextLocation.x + nextLocation.width) -(prevLocation.x + prevLocation.width);

        if (i == 0 || i == 2) {
            /* Checking that separators (:, ' and ") aren't detected as digits.
             * For that we check that the digits adjacent to the separator have increased interval. */
            if (interval <= 26) {
                digitMatches.erase(digitMatches.begin() + i + 1);
                i--;
            }
        }
        else {
            // Checking that the interval is not too high.
            if (interval > 20) {
                return false;
            }
        }
    }

    int requiredDigitsCount;
    if (timeIncludesMilliseconds())
        requiredDigitsCount = 5;
    else
        requiredDigitsCount = 3;

    if (settings.isComposite) {
        while (digitMatches.size() > requiredDigitsCount && digitMatches.back().templateName == "1")
            digitMatches.pop_back();
    }

    if (digitMatches.size() != requiredDigitsCount) {
        return false;
    }
    return true;
}


void TimeRecognizer::getTimeFromRecognizedDigits(const std::vector<TemplateMatcher::Match>& digitMatches, AnalysisResult& result) {
    std::string timeDigitsStr;
    for (const TemplateMatcher::Match& match : digitMatches)
        timeDigitsStr += match.templateName;

    // Formatting the timeString and calculating timeInMilliseconds
    std::string minutes = timeDigitsStr.substr(0, 1), seconds = timeDigitsStr.substr(1, 2);
    if (std::stoi(seconds) >= 60) {
        // The number of seconds is always 59 or lower.
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }

    result.timeInMilliseconds = std::stoi(minutes) * 60 * 1000 + std::stoi(seconds) * 1000;
    if (timeIncludesMilliseconds()) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;
        result.timeString += std::format("{}'{}\"{}", minutes, seconds, milliseconds);
    }
    else {
        result.timeString = std::format("{}:{}", minutes, seconds);
    }

    result.recognizedTime = true;
    result.errorReason = ErrorReasonEnum::NO_ERROR;
}


bool TimeRecognizer::doCheckForScoreScreen(std::vector<TemplateMatcher::Match>& labels, int originalFrameHeight) {
    std::vector<TemplateMatcher::Match> timeMatches, scoreMatches;
    for (const TemplateMatcher::Match& match : labels) {
        if (match.templateName == TemplateMatcher::TIME)
            timeMatches.push_back(match);
        else
            scoreMatches.push_back(match);
    }

    if (timeMatches.empty() || scoreMatches.empty())
        return false;
    auto topTimeLabel = findTopTimeLabel(labels);

    // Location of TIME in "TIME BONUS" relative to the top time label.
    cv::Point2f expectedTimeBonusShift;
    switch (settings.game) {
    case Game::Sonic1:
        expectedTimeBonusShift = cv::Point2f(5.82f, 8.36f);
        break;
    case Game::Sonic2:
        expectedTimeBonusShift = cv::Point2f(4.73f, 8.f);
        break;
    case Game::SonicCD:
        expectedTimeBonusShift = cv::Point2f(4.73f, 12.36f);
        break;
    }
    expectedTimeBonusShift *= topTimeLabel.location.height;

    int maxDifference = topTimeLabel.location.height * 3;

    // Make sure that the other TIME matches are valid.
    std::erase_if(timeMatches, [&](const auto& timeMatch) {
        if (timeMatch == topTimeLabel)
            return false;
        const cv::Rect& topLocation = topTimeLabel.location;
        const cv::Rect& otherLocation = timeMatch.location;
        cv::Point2f timeBonusShift = otherLocation.tl() - topLocation.tl();
        float difference = (float) cv::norm(timeBonusShift - expectedTimeBonusShift);
        return difference > maxDifference;
    });

    /* There's always a "TIME" label on top of the screen.
     * But there's a second one during the score countdown screen ("TIME BONUS"). */
    bool isScoreScreen = timeMatches.size() >= 2;
    labels = std::move(timeMatches);
    labels.insert(labels.end(), scoreMatches.begin(), scoreMatches.end());
    return isScoreScreen;
}


void TimeRecognizer::onRecognitionSuccess() {
    lastSuccessfulDigitsLocation = curDigitsLocation;
    lastRecognitionSuccessTime = std::chrono::steady_clock::now();
}


void TimeRecognizer::onRecognitionFailure(AnalysisResult& result) {
    result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
    if (recalculatedBestScaleLastTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        resetDigitsLocation();
    }
    else {
        curDigitsLocation = lastSuccessfulDigitsLocation;
    }
}


void TimeRecognizer::updateDigitsRect(const std::vector<TemplateMatcher::Match>& labels) {
    cv::Rect timeRect = findTopTimeLabel(labels).location;
    curDigitsLocation.timeRect = timeRect;
    double rightBorderCoefficient = (settings.game == Game::SonicCD ? 3.2 : 2.45);
    int digitsRectLeft = timeRect.x + static_cast<int>(timeRect.width * 1.22);
    int digitsRectRight = timeRect.x + static_cast<int>(timeRect.width * rightBorderCoefficient);
    int digitsRectTop = timeRect.y - static_cast<int>(timeRect.height * 0.1);
    int digitsRectBottom = timeRect.y + static_cast<int>(timeRect.height * 1.1);
    curDigitsLocation.digitsRect = {digitsRectLeft, digitsRectTop,
        digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
}


TemplateMatcher::Match TimeRecognizer::findTopTimeLabel(const std::vector<TemplateMatcher::Match>& labels) {
    auto timeMatches = labels | std::views::filter([](const TemplateMatcher::Match& match) {
        return match.templateName == TemplateMatcher::TIME;
    });
    auto topTimeLabel = std::ranges::min(timeMatches, {}, [](const TemplateMatcher::Match& timeMatch) {
        return timeMatch.location.y;
    });
    return topTimeLabel;
}


/* Returns all found positions of a template.
 * Goes through different scales of the original frame (see the link below).
 * Modifies frame in-place to be scaled to bestScale.
 * Algorithm based on: https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv */
std::vector<TemplateMatcher::Match> TimeRecognizer::calculateBestScaleAndFindMatches(cv::UMat& frame, const std::string& templateName, bool croppedToGameRect) {
    // if we already found the scale, we don't go through different scales
    double minScale, maxScale;

    if (croppedToGameRect) {
        minScale = maxScale = 224. * 2 / frame.rows;  // Height should be double the Genesis resolution height
    } else {
        /* Sega Genesis resolution is 320×224 (height = 224).
            * We double the size of the template images, so we have to do the same for the resolution. */
        const int minHeight = 200 * 2;
        const int maxHeight = 320 * 2;
        /* 224 / 320 = 0.7
            * Thus the frame should take at least 70% of stream's height. (80% for safety). */
        minScale = ((double) minHeight) / frame.rows;
        maxScale = ((double) maxHeight) / frame.rows;
    }

    double bestSimilarityForScales = -1e10;
    // bestSimilarityForScales is used only if recalculateBestScale is true.

    std::vector<TemplateMatcher::Match> matches;
    cv::UMat resizedToBestScale;

    for (double scale = maxScale; scale >= minScale; scale *= 0.96) {
        cv::UMat resized;
        if (scale != 1)
            cv::resize(frame, resized, {}, scale, scale, cv::INTER_AREA);
        else
            resized = frame;

        std::vector<TemplateMatcher::Match> matchesForScale = 
            templateMatcher.findTemplateLocations(resized, std::vector<std::string>{templateName}, false);
        if (matchesForScale.empty()) continue;

        double bestSimilarityForScale =
            std::ranges::max(matchesForScale | std::views::transform([](const TemplateMatcher::Match& match) { return match.similarity; }));
        if (bestSimilarityForScales < bestSimilarityForScale) {
            bestSimilarityForScales = bestSimilarityForScale;
            curDigitsLocation.bestScale = scale;
            matches = std::move(matchesForScale);
            resizedToBestScale = std::move(resized);
        }
    }

    if (!matches.empty()) {
        // Save one cv::resize operation by returning its result the caller
        frame = resizedToBestScale;
    }

    return matches;
}


cv::UMat TimeRecognizer::cropToDigitsRect(const cv::UMat& frame) const {
    // Checking that digitsRect is inside the frame.
    cv::Rect frameRect({0, 0}, frame.size());
    const cv::Rect& digitsRect = curDigitsLocation.digitsRect;
    if ((digitsRect & frameRect) != digitsRect) {
        return {};
    }
    return frame(digitsRect);
}


cv::UMat TimeRecognizer::applyColorCorrection(cv::UMat img) const {
    if (img.empty())
        return {};
    img = TemplateMatcher::convertToGray(img, false);

    std::vector<uint8_t> pixels;
    cv::Mat imgMat = img.getMat(cv::ACCESS_READ);
    for (int y = 0; y < img.rows; y += 2) {
        for (int x = 0; x < img.cols; x += 2) {
            pixels.push_back((uint8_t) imgMat.at<float>(y, x));
        }
    }
    std::ranges::sort(pixels);

    const float darkPosition = 0.25f, brightPosition = 0.85f, veryBrightPosition = 0.98f;
    uint8_t minBrightness = pixels[(int) (pixels.size() * darkPosition)];
    uint8_t maxBrightness = pixels[(int) (pixels.size() * brightPosition)];
    uint8_t whiteColor = pixels[(int) (pixels.size() * veryBrightPosition)];

    if (whiteColor < 175) {
        // The image is too dark. In Sonic games transitions to black are happening after the timer has stopped anyways.
        return {};
    }

    // We only need to recognize time before transition to white on Sonic 2's Death Egg.
    bool recognizeWhiteTransition = (settings.game == Game::Sonic2 && currentSplitIndex == 19);

    if (!recognizeWhiteTransition) {
        uint8_t maxAcceptableBrightness;
        if (settings.game == Game::Sonic2) {
            // Sonic 2's Chemical Plant is white enough, so we accept any brightness.
            maxAcceptableBrightness = 255;
        } else if (settings.game == Game::SonicCD && currentSplitIndex == 20) {
            // In the ending of Sonic CD the screen goes white, and we don't wanna try to recognize that.
            maxAcceptableBrightness = 90;
        } else {
            maxAcceptableBrightness = 180;
        }

        if (minBrightness > maxAcceptableBrightness)
            return {};
        minBrightness = std::min(minBrightness, (uint8_t) 50);
    }
    maxBrightness = std::max(maxBrightness, (uint8_t) 210);

    uint8_t difference = maxBrightness - minBrightness;
    const uint8_t MIN_DIFFERENCE = 4;

    if (difference < MIN_DIFFERENCE) {
        // This is an almost single-color image, there's no point in increasing the contrast.
        return {};
    }

    // Making the minimum brightness equal to 0 and the maximum brightness equal to newMaxBrighteness.
    const uint8_t newMaxBrightness = 230;  // The maximum brightness for digits.
    cv::subtract(img, cv::Scalar((float) minBrightness), img);
    cv::multiply(img, cv::Scalar((float) newMaxBrightness / difference), img);

    // Making sure all pixels are within the range of 0 to newMaxBrightness
    cv::threshold(img, img, 0, 0, cv::THRESH_TOZERO);
    cv::threshold(img, img, newMaxBrightness, 0, cv::THRESH_TRUNC);

    return img;
}


bool TimeRecognizer::timeIncludesMilliseconds() const {
    return settings.game == Game::SonicCD;
}


cv::Rect TimeRecognizer::scaleAndRound(cv::Rect src, double scale) {
    return {(int) std::round(src.x * scale), (int) std::round(src.y * scale), 
        (int) std::round(src.width * scale), (int) std::round(src.height * scale)};
}


}  // namespace IGT
}  // namespace SonicVisualSplitBase
