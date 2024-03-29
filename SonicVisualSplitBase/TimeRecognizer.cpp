﻿#include "TimeRecognizer.h"
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


TimeRecognizer::TimeRecognizer(const AnalysisSettings& settings) : 
        settings(settings), onSourceChangedListener(*this) {
    loadAllTemplates();
    FrameStorage::addOnSourceChangedListener(onSourceChangedListener);
}


TimeRecognizer::~TimeRecognizer() {
    FrameStorage::removeOnSourceChangedListener(onSourceChangedListener);
}


std::vector<TimeRecognizer::Match> TimeRecognizer::recognizeTime
        (cv::UMat frame, bool checkForScoreScreen, AnalysisResult& result) {
    if (shouldResetDigitsLocation) {
        resetDigitsLocationSync();
        shouldResetDigitsLocation = false;
    }

    if (frame.size() != curDigitsLocation.frameSize) {
        resetDigitsLocationSync();
        curDigitsLocation.frameSize = frame.size();
    }

    std::vector<Match> allMatches;

    if (curDigitsLocation.isValid()) {
        cv::resize(frame, frame, {}, curDigitsLocation.bestScale, curDigitsLocation.bestScale);
        recalculatedBestScaleLastTime = false;
    }
    else {
        // When we're looking for the labelMatches, we're recalculating the best scale (if needed) and the digitsRect.
        checkForScoreScreen = true;
    }
    if (checkForScoreScreen) {
        std::vector<Match> labels = findLabelsAndUpdateDigitsRect(frame);
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

    std::vector<Match> digitMatches;
    for (char digit = '0'; digit <= '9'; digit++) {
        std::vector<Match> matches = findSymbolLocations(frame, digit, false);

        for (Match& match : matches) {
            // The frame is cropped to digitsRect to speed up the search. Now we have to compensate for that.
            match.location += cv::Point2f((float) (curDigitsLocation.digitsRect.x / curDigitsLocation.bestScale),
                (float) (curDigitsLocation.digitsRect.y / curDigitsLocation.bestScale));
        }
        digitMatches.insert(digitMatches.end(), matches.begin(), matches.end());
    }

    removeMatchesWithLowSimilarity(digitMatches);
    removeMatchesWithIncorrectYCoord(digitMatches);
    removeOverlappingMatches(digitMatches);

    if (checkRecognizedDigits(digitMatches))
        getTimeFromRecognizedDigits(digitMatches, result);

    if (result.recognizedTime)
        onRecognitionSuccess();
    else
        onRecognitionFailure(result);

    allMatches.insert(allMatches.end(), digitMatches.begin(), digitMatches.end());
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


void TimeRecognizer::OnSourceChangedListenerImpl::onSourceChanged() const {
    timeRecognizer.resetDigitsLocation();
}


TimeRecognizer::DigitsLocation TimeRecognizer::getLastSuccessfulDigitsLocation(){
    return lastSuccessfulDigitsLocation;
}


std::chrono::steady_clock::duration TimeRecognizer::getTimeSinceDigitsLocationLastUpdated() {
    return std::chrono::steady_clock::now() - lastRecognitionSuccessTime;
}


void TimeRecognizer::reportCurrentSplitIndex(int currentSplitIndex) {
    this->currentSplitIndex = currentSplitIndex;
}


std::vector<TimeRecognizer::Match> TimeRecognizer::findLabelsAndUpdateDigitsRect(cv::UMat frame) {
    // Template matching for TIME and SCORE is done on the grayscale frame where yellow is white (it's more accurate this way).
    cv::UMat frameWithYellowFilter = convertFrameToGray(frame, true);
    bool recalculateBestScale = (curDigitsLocation.bestScale == -1);
    recalculatedBestScaleLastTime = recalculateBestScale;
    std::vector<Match> timeMatches = findSymbolLocations(frameWithYellowFilter, TIME, recalculateBestScale);
    if (timeMatches.empty())
        return {};

    // Checking only the top half of the frame, so that "SCORE" is searched faster.
    cv::Rect topHalf = {0, 0, frame.cols, frame.rows / 2};
    frameWithYellowFilter = frameWithYellowFilter(topHalf);
    if (recalculateBestScale) {
        // bestScale is calculated already after we found "TIME".
        cv::resize(frameWithYellowFilter, frameWithYellowFilter, {},
            curDigitsLocation.bestScale, curDigitsLocation.bestScale, cv::INTER_AREA);
    }
    std::vector<Match> scoreMatches = findSymbolLocations(frameWithYellowFilter, SCORE, false);
    if (scoreMatches.empty())
        return {};

    std::vector<Match> labelMatches = std::move(timeMatches);
    labelMatches.insert(labelMatches.end(), scoreMatches.begin(), scoreMatches.end());
    removeMatchesWithLowSimilarity(labelMatches);
    removeOverlappingMatches(labelMatches);
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


bool TimeRecognizer::checkRecognizedDigits(std::vector<Match>& digitMatches) {
    std::ranges::sort(digitMatches, {}, [](const Match& match) {
        return match.location.x;
    });

    for (int i = 0; i + 1 < digitMatches.size(); i++) {
        const cv::Rect2f& prevLocation = digitMatches[i].location, nextLocation = digitMatches[i + 1].location;
        double interval = ((nextLocation.x + nextLocation.width) -
            (prevLocation.x + prevLocation.width)) * curDigitsLocation.bestScale;

        if (i == 0 || i == 2) {
            /* Checking that separators (:, ' and ") aren't detected as digits.
             * For that we check that the digits adjacent to the separator have increased interval. */
            if (interval < 26.5) {
                digitMatches.erase(digitMatches.begin() + i + 1);
                i--;
            }
        }
        else {
            // Checking that the interval is not too high.
            if (interval > 20.5) {
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
        while (digitMatches.size() > requiredDigitsCount && digitMatches.back().symbol == '1')
            digitMatches.pop_back();
    }

    if (digitMatches.size() != requiredDigitsCount) {
        return false;
    }
    return true;
}


void TimeRecognizer::getTimeFromRecognizedDigits(const std::vector<Match>& digitMatches, AnalysisResult& result) {
    std::string timeDigitsStr;
    for (const Match& match : digitMatches)
        timeDigitsStr += match.symbol;

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


bool TimeRecognizer::doCheckForScoreScreen(std::vector<Match>& labels, int originalFrameHeight) {
    std::vector<Match> timeMatches, scoreMatches;
    for (const Match& match : labels) {
        if (match.symbol == TIME)
            timeMatches.push_back(match);
        else
            scoreMatches.push_back(match);
    }
    
    if (timeMatches.empty() || scoreMatches.empty())
        return false;
    auto topTimeLabel = findTopTimeLabel(labels);
    
    // Location of TIME in "TIME BONUS" relative to the top time label.
    cv::Point2f expectedTimeBonusShift;
    if (settings.gameName == "Sonic 1")
        expectedTimeBonusShift = cv::Point2f(5.82f, 8.36f);
    else if (settings.gameName == "Sonic 2")
        expectedTimeBonusShift = cv::Point2f(4.73f, 8.f);
    else if (settings.gameName == "Sonic CD")
        expectedTimeBonusShift = cv::Point2f(4.73f, 12.36f);
    expectedTimeBonusShift *= topTimeLabel.location.height;

    float maxDifference = topTimeLabel.location.height * 3;

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


void TimeRecognizer::updateDigitsRect(const std::vector<Match>& labels) {
    cv::Rect2f timeRectScaled = findTopTimeLabel(labels).location;
    const double& bestScale = curDigitsLocation.bestScale;
    auto& timeRect = curDigitsLocation.timeRect;
    timeRect = {(int) std::round(timeRectScaled.x * bestScale), (int) std::round(timeRectScaled.y * bestScale),
        (int) std::round(timeRectScaled.width * bestScale), (int) std::round(timeRectScaled.height * bestScale)};
    double rightBorderCoefficient = (settings.gameName == "Sonic CD" ? 3.2 : 2.45);
    int digitsRectLeft = (int) (timeRect.x + timeRect.width * 1.22);
    int digitsRectRight = (int) (timeRect.x + timeRect.width * rightBorderCoefficient);
    int digitsRectTop = (int) (timeRect.y - timeRect.height * 0.1);
    int digitsRectBottom = (int) (timeRect.y + timeRect.height * 1.1);
    curDigitsLocation.digitsRect = {digitsRectLeft, digitsRectTop,
        digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
}


TimeRecognizer::Match TimeRecognizer::findTopTimeLabel(const std::vector<Match>& labels) {
    auto timeMatches = labels | std::views::filter([](const Match& match) {
        return match.symbol == TIME;
    });
    auto topTimeLabel = std::ranges::min(timeMatches, {}, [](const Match& timeMatch) {
        return timeMatch.location.y;
    });
    return topTimeLabel;
}


/* Returns all found positions of a digit. If recalculateBestScale is set to true,
 * goes through different scales of the original frame (see the link below).
 * Algorithm based on: https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv */
std::vector<TimeRecognizer::Match> TimeRecognizer::findSymbolLocations(cv::UMat frame, char symbol, bool recalculateBestScale) {
    const auto& [templateImage, templateMask, opaquePixels] = templates[symbol];

    // if we already found the scale, we don't go through different scales
    double minScale, maxScale;

    if (recalculateBestScale) {
        /* Sega Genesis resolution is 320×224 (height = 224).
         * We double the size of the template images, so we have to do the same for the resolution. */
        const int minHeight = 200 * 2;
        const int maxHeight = 320 * 2;
        /* 224 / 320 = 0.7
         * Thus the frame should take at least 70% of stream's height. (80% for safety). */
        minScale = ((double) minHeight) / frame.rows;
        maxScale = ((double) maxHeight) / frame.rows;
    }
    else {
        minScale = maxScale = 1;
    }

    double globalMinSimilarity = getMinSimilarity(symbol);
    double bestSimilarityForScales = globalMinSimilarity;
    // bestSimilarityForScales is used only if recalculateBestScale is true.

    std::vector<Match> matches;  // Pairs: {supposed digit location, similarity coefficient}.

    for (double scale = maxScale; scale >= minScale; scale *= 0.96) {
        cv::UMat resized;
        if (scale != 1)
            cv::resize(frame, resized, {}, scale, scale, cv::INTER_AREA);
        else
            resized = frame;

        if (resized.cols < templateImage.cols || resized.rows < templateImage.rows)
            continue;
        cv::UMat matchResult;
        cv::matchTemplate(resized, templateImage, matchResult, cv::TM_SQDIFF, templateMask);

        if (recalculateBestScale) {
            double minimumSqdiff;
            cv::minMaxLoc(matchResult, &minimumSqdiff);
            double bestSimilarityForScale = -minimumSqdiff / opaquePixels;
            if (bestSimilarityForScales < bestSimilarityForScale) {
                bestSimilarityForScales = bestSimilarityForScale;
                curDigitsLocation.bestScale = scale;
            }
            else {
                // This is not the best scale.
                continue;
            }
        }

        matches.clear();

        cv::UMat matchResultBinary;
        cv::threshold(matchResult, matchResultBinary, -globalMinSimilarity * opaquePixels, 1, cv::THRESH_BINARY_INV);
        std::vector<cv::Point> matchPoints;
        cv::findNonZero(matchResultBinary, matchPoints);

        cv::Mat matchResultRead = matchResult.getMat(cv::ACCESS_READ);

        for (const cv::Point& match : matchPoints) {
            int x = match.x, y = match.y;
            /* If the method of template matching is square difference (TM_SQDIFF),
             * then the lower value is a better match.
             * We want the higher similarity to be a better match, so we negate the square difference. */
            double similarity = -matchResultRead.at<float>(y, x) / opaquePixels;

            if (symbol == '1') {
                // We want the right edge to be aligned with the 8x8 tile of the digit.
                x += 2;
            }

            // The frame is resized to the bestScale, so the match rectangle will be different.
            const double& bestScale = curDigitsLocation.bestScale;
            cv::Rect2f matchRect((float) (x / bestScale), (float) (y / bestScale),
                (float) (templateImage.cols / bestScale), (float) (templateImage.rows / bestScale));
            matches.push_back({matchRect, symbol, similarity});
        }
    }

    for (Match& match : matches) {
        match.similarity *= getSimilarityMultiplier(symbol);
    }
    return matches;
}


void TimeRecognizer::removeMatchesWithLowSimilarity(std::vector<Match>& matches) {
    std::erase_if(matches, [&](const Match& match) {
        double minSimilarity = getMinSimilarity(match.symbol);
        double similarityMultiplier = getSimilarityMultiplier(match.symbol);
        return match.similarity / similarityMultiplier < minSimilarity;
    });
}


void TimeRecognizer::removeOverlappingMatches(std::vector<Match>& matches) {
    // Sorting the matches by similarity in descending order, and removing the overlapping ones.
    std::ranges::sort(matches, std::greater<>(), &Match::similarity);
    std::vector<Match> resultSymbolLocations;

    for (const auto& match : matches) {
        bool intersectsWithOthers = false;

        for (const Match& other : resultSymbolLocations) {
            if (std::isdigit(match.symbol) && std::isdigit(other.symbol)) {
                if (std::abs(match.location.x + match.location.width -
                        (other.location.x + other.location.width)) * curDigitsLocation.bestScale < 12) {
                    intersectsWithOthers = true;
                }
            }
            else if (!(match.location & other.location).empty()) {
                intersectsWithOthers = true;
            }

            if (intersectsWithOthers)
                break;
        }

        if (!intersectsWithOthers)
            resultSymbolLocations.push_back(match);
    }

    matches = resultSymbolLocations;
}


void TimeRecognizer::removeMatchesWithIncorrectYCoord(std::vector<Match>& digitMatches) {
    if (digitMatches.empty())
        return;
    const Match& bestMatch = std::ranges::max(digitMatches, {}, &Match::similarity);
    const cv::Rect2f& bestMatchLocation = bestMatch.location;

    std::erase_if(digitMatches, [&](const Match& match) {
        // If the difference in the y coordinates is more than one pixel, we consider it a wrong match.
        return std::abs(match.location.y - bestMatchLocation.y)
            * curDigitsLocation.bestScale > 1.5;
    });
}


double TimeRecognizer::getMinSimilarity(char symbol) const {
    if (std::isdigit(symbol)) {
        double baselineMinSimilarity = (settings.isComposite ? -8000 : -7000);
        /* If a multiplier is present, that means that the symbol tends to get false matches.
         * Therefore the minimum acceptable similarity is lowered. */
        double multiplier = getSimilarityMultiplier(symbol);
        multiplier = std::min(multiplier, 1.5);
        return baselineMinSimilarity / multiplier;
    }
    else {
        // TIME and SCORE labelMatches.
        if (settings.isComposite)
            return -4500;
        else
            return -7000;
    }
}


double TimeRecognizer::getSimilarityMultiplier(char symbol) const {
    switch (symbol)
    {
    // No multiplier by default.
    default:
        return 1;
    // Once again making one a less preferable option.
    case '1':
        return 2;
    // Four is confused with one.
    case '4':
        return 1.2;
    // Seven is confused with one and two.
    case '7':
        return 1.15;
    // Nine is confused with five.
    case '9':
        if (settings.isComposite && settings.gameName != "Sonic 2")
            return 1.15;
        else
            return 1;
    }
}


cv::UMat TimeRecognizer::cropToDigitsRect(cv::UMat frame) {
    // Checking that digitsRect is inside the frame.
    cv::Rect frameRect({0, 0}, frame.size());
    const cv::Rect& digitsRect = curDigitsLocation.digitsRect;
    if ((digitsRect & frameRect) != digitsRect) {
        return {};
    }
    frame = frame(digitsRect);
    return frame;
}


cv::UMat TimeRecognizer::applyColorCorrection(cv::UMat img) {
    if (img.empty())
        return {};
    img = convertFrameToGray(img);

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
    bool recognizeWhiteTransition = (settings.gameName == "Sonic 2" && currentSplitIndex == 19);
    
    if (!recognizeWhiteTransition) {
        uint8_t maxAcceptableBrightness;
        if (settings.gameName == "Sonic 2") {
            // Sonic 2's Chemical Plant is white enough, so we accept any brightness.
            maxAcceptableBrightness = 255;
        }
        else if (settings.gameName == "Sonic CD" && currentSplitIndex == 20) {
            // In the ending of Sonic CD the screen goes white, and we don't wanna try to recognize that.
            maxAcceptableBrightness = 90;
        }
        else {
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


cv::UMat TimeRecognizer::convertFrameToGray(cv::UMat frame, bool filterYellowColor) {
    cv::UMat result;

    if (!filterYellowColor) {
        auto conversionType = (frame.channels() == 3 ? cv::COLOR_BGR2GRAY : cv::COLOR_BGRA2GRAY);
        cv::cvtColor(frame, result, conversionType);
    }
    else {
        /* Getting the yellow color intensity by adding green and red channels (BGR).
         * TIME and SCORE are yellow, so it's probably a good way to convert a frame to grayscale. */
        std::vector<cv::UMat> channels(4);
        cv::split(frame, channels);
        cv::addWeighted(channels[1], 0.5, channels[2], 0.5, 0, result);
    }

    result.convertTo(result, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.
    return result;
}


bool TimeRecognizer::timeIncludesMilliseconds() const {
    return settings.gameName == "Sonic CD";
}


void TimeRecognizer::loadAllTemplates(){
    std::vector<char> symbolsToLoad = {TIME, SCORE};
    for (char digit = '0'; digit <= '9'; digit++)
        symbolsToLoad.push_back(digit);

    for (char symbol : symbolsToLoad) {
        templates[symbol] = loadSymbolTemplate(symbol);
    }
}


std::tuple<cv::UMat, cv::UMat, int> TimeRecognizer::loadSymbolTemplate(char symbol) {
    cv::UMat image = settings.loadTemplateImageFromFile(symbol);
    // We double the size of the image, as then matching is more accurate.
    cv::resize(image, image, {}, 2, 2, cv::INTER_NEAREST);

    bool filterYellowColor = (symbol == TIME || symbol == SCORE);
    cv::UMat grayImage = convertFrameToGray(image, filterYellowColor);

    cv::UMat templateMask = getAlphaMask(image);
    int opaquePixels = cv::countNonZero(templateMask);

    return {grayImage, templateMask, opaquePixels};
}


cv::UMat TimeRecognizer::getAlphaMask(cv::UMat image) {
    std::vector<cv::UMat> channels(4);
    cv::split(image, channels);

    cv::UMat mask = channels[3];  // Get the alpha channel.
    cv::threshold(mask, mask, 0, 1.0, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_32F);  // Converting to CV_32F since matchTemplate does that anyway.

    return mask;
}

bool TimeRecognizer::Match::operator==(const Match& other) const = default;

}