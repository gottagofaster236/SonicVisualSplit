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
#include <cassert>


namespace SonicVisualSplitBase {


TimeRecognizer::TimeRecognizer(FrameAnalyzer& frameAnalyzer, const std::string& gameName,
        const std::filesystem::path& templatesDirectory, bool isComposite)
            : frameAnalyzer(frameAnalyzer), gameName(gameName), templatesDirectory(templatesDirectory),
              isComposite(isComposite), onSourceChangedListener(*this) {
    std::vector<char> symbolsToLoad = {TIME, SCORE};
    for (char digit = '0'; digit <= '9'; digit++)
        symbolsToLoad.push_back(digit);
    for (char symbol : symbolsToLoad) {
        templates[symbol] = loadImageAndMaskFromFile(symbol);
    }

    FrameStorage::addOnSourceChangedListener(onSourceChangedListener);
}


TimeRecognizer::~TimeRecognizer() {
    FrameStorage::removeOnSourceChangedListener(onSourceChangedListener);
}


std::vector<TimeRecognizer::Match> TimeRecognizer::recognizeTime
        (cv::UMat frame, bool checkForScoreScreen, AnalysisResult& result) {
    if (shouldResetDigitsPlacement) {
        resetDigitsPlacementSync();
        shouldResetDigitsPlacement = false;
    }

    if (frame.size() != lastFrameSize) {
        resetDigitsPlacementSync();
        lastFrameSize = frame.size();
    }
    prevDigitsRect = digitsRect;

    std::vector<Match> allMatches;

    if (bestScale != -1) {
        cv::resize(frame, frame, {}, bestScale, bestScale);
        recalculatedBestScaleLastTime = false;
    }
    if (bestScale == -1 || digitsRect.empty()) {
        // When we're looking for the labelMatches, we're recalculating the best scale (if needed) and the digitsRect.
        checkForScoreScreen = true;
    }
    if (checkForScoreScreen) {
        std::vector<Match> labels = findLabelsAndUpdateDigitsRect(frame);
        if (digitsRect.empty())
            digitsRect = prevDigitsRect;
        if (bestScale == -1 || digitsRect.empty()) {
            onRecognitionFailure(result);
            return allMatches;
        }
        if (recalculatedBestScaleLastTime)
            cv::resize(frame, frame, {}, bestScale, bestScale);
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
            match.location += cv::Point2f((float) (digitsRect.x / bestScale), (float) (digitsRect.y / bestScale));
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


void TimeRecognizer::resetDigitsPlacement() {
    // Digits placement will be reset on the next call to recognizeTime.
    shouldResetDigitsPlacement = true;
}


void TimeRecognizer::resetDigitsPlacementSync() {
    bestScale = -1;
    digitsRect = {0, 0, 0, 0};
}


TimeRecognizer::OnSourceChangedListenerImpl::OnSourceChangedListenerImpl(TimeRecognizer& timeRecognizer)
    : timeRecognizer(timeRecognizer) {}


void TimeRecognizer::OnSourceChangedListenerImpl::onSourceChanged() const {
    timeRecognizer.resetDigitsPlacement();
}


double TimeRecognizer::getBestScale() const {
    return bestScale;
}


cv::Rect2f TimeRecognizer::getRelativeTimeRect() {
    auto currentTime = std::chrono::steady_clock::now();
    if (currentTime < relativeTimeRectUpdatedTime + std::chrono::seconds(10)) {
        return relativeTimeRect;
    }
    else {
        // We consider the relative digits rectangle outdated now, it hasn't been updated for too long.
        return {};
    }
}


cv::Rect TimeRecognizer::getTimeRectFromFrameSize(cv::Size frameSize) {
    cv::Rect2f relativeTimeRect = getRelativeTimeRect();
    cv::Rect timeRect = {
        (int) std::round(relativeTimeRect.x * frameSize.width),
        (int) std::round(relativeTimeRect.y * frameSize.height),
        (int) std::round(relativeTimeRect.width * frameSize.width),
        (int) std::round(relativeTimeRect.height * frameSize.height)
    };
    return timeRect;
}


std::vector<TimeRecognizer::Match> TimeRecognizer::findLabelsAndUpdateDigitsRect(cv::UMat frame) {
    // Template matching for TIME and SCORE is done on the grayscale frame where yellow is white (it's more accurate this way).
    cv::UMat frameWithYellowFilter = convertFrameToGray(frame, true);
    bool recalculateBestScale = (bestScale == -1);
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
            bestScale, bestScale, cv::INTER_AREA);
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
            (prevLocation.x + prevLocation.width)) * getBestScale();

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

    if (isComposite) {
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
    result.timeString = minutes + "'" + seconds;
    result.timeInMilliseconds = std::stoi(minutes) * 60 * 1000 + std::stoi(seconds) * 1000;
    if (timeIncludesMilliseconds()) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeString += '"' + milliseconds;
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;
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
    if (gameName == "Sonic 1")
        expectedTimeBonusShift = cv::Point2f(5.82f, 8.36f);
    else if (gameName == "Sonic 2")
        expectedTimeBonusShift = cv::Point2f(4.73f, 8.f);
    else if (gameName == "Sonic CD")
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
    int frameWidth = lastFrameSize.width;
    int frameHeight = lastFrameSize.height;
    relativeTimeRect = {timeRect.x / frameWidth, timeRect.y / frameHeight,
        timeRect.width / frameWidth, timeRect.height / frameHeight};
    relativeTimeRect &= cv::Rect2f(0, 0, 1, 1);  // Make sure it is not out of bounds.
    relativeTimeRectUpdatedTime = std::chrono::steady_clock::now();
}


void TimeRecognizer::onRecognitionFailure(AnalysisResult& result) {
    result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
    if (recalculatedBestScaleLastTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        resetDigitsPlacement();
    }
    else {
        digitsRect = prevDigitsRect;
    }
}


void TimeRecognizer::updateDigitsRect(const std::vector<Match>& labels) {
    timeRect = findTopTimeLabel(labels).location;
    double rightBorderCoefficient = (gameName == "Sonic CD" ? 3.2 : 2.45);
    int digitsRectLeft = (int) ((timeRect.x + timeRect.width * 1.22) * bestScale);
    int digitsRectRight = (int) ((timeRect.x + timeRect.width * rightBorderCoefficient) * bestScale);
    int digitsRectTop = (int) ((timeRect.y - timeRect.height * 0.1) * bestScale);
    int digitsRectBottom = (int) ((timeRect.y + timeRect.height * 1.1) * bestScale);
    digitsRect = {digitsRectLeft, digitsRectTop, digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
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

    double globalMinSimilarity = getGlobalMinSimilarity(symbol);
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
                bestScale = scale;
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
            cv::Rect2f matchRect((float) (x / bestScale), (float) (y / bestScale),
                (float) (templateImage.cols / bestScale), (float) (templateImage.rows / bestScale));
            matches.push_back({matchRect, symbol, similarity});
        }
    }

    for (Match& match : matches) {
        match.similarity *= getSimilarityMultiplier(symbol);
    }

    // DEBUG STUFF BELOW
    if (symbol == TIME && !matches.empty() && !recalculateBestScale) {
        cv::Rect2f bestMatch = std::ranges::max(matches, {}, [](const Match& match) {
            return match.similarity;
        }).location;
        cv::Rect actualMatch = {
            (int) std::round(bestMatch.x * bestScale),
            (int) std::round(bestMatch.y * bestScale),
            (int) std::round(bestMatch.width * bestScale),
            (int) std::round(bestMatch.height * bestScale)
        };
        cv::imwrite("C:/tmp/theMostAccurateTimeRect.png", frame(actualMatch));
    }

    return matches;
}


void TimeRecognizer::removeMatchesWithLowSimilarity(std::vector<Match>& matches) {
    if (matches.size() < 2)
        return;
    // Finding the second largest similarity (not the first for precision).
    std::ranges::nth_element(matches, matches.begin() + 1, std::greater<>(), &Match::similarity);
    double bestSimilarity = matches[1].similarity;
    
    std::erase_if(matches, [&](const Match& match) {
        double similarityCoefficient = getMinSimilarityDividedByBestSimilarity(match.symbol);
        double minSimilarity = bestSimilarity * similarityCoefficient;
        double globalMinSimilarity = getGlobalMinSimilarity(match.symbol);
        minSimilarity = std::max(minSimilarity, globalMinSimilarity);
        minSimilarity = std::min(minSimilarity, -1500.);  // Make sure our constraint is possible to meet.
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
                        (other.location.x + other.location.width)) * bestScale < 12) {
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
        return std::abs(match.location.y - bestMatchLocation.y) * bestScale > 1.5;
    });
}


double TimeRecognizer::getGlobalMinSimilarity(char symbol) const {
    if (std::isdigit(symbol)) {
        double baselineMinSimilarity = (isComposite ? -8000 : -7000);
        /* If a multiplier is present, that means that the symbol tends to get false matches.
         * Therefore the minimum acceptable similarity is lowered. */
        double multiplier = getSimilarityMultiplier(symbol);
        multiplier = std::min(multiplier, 1.5);
        return baselineMinSimilarity / multiplier;
    }
    else {
        // TIME and SCORE labelMatches.
        if (isComposite)
            return -3500;
        else
            return -7000;
    }
}


double TimeRecognizer::getMinSimilarityDividedByBestSimilarity(char symbol) const {
    switch (symbol) {
    default:
        return 3.25;
    case SCORE:
        return 2;
    // We use "TIME" to detect the score screen, so we want to be sure.
    case TIME:
        if (isComposite)
            return 1.75;
        else
            return 2;
    /* One is really small, so it can be misdetected, therefore the coefficient is lowered.
     * This leads to four recognizing instead of one - so coefficient for four is lowered too. */
    case '1':
        if (isComposite)
            return 2;
        else
            return 3;
    case '4':
        if (isComposite)
            return 2;
        else
            return 2.5;
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
        if (isComposite && gameName != "Sonic 2")
            return 1.15;
        else
            return 1;
    }
}


cv::UMat TimeRecognizer::cropToDigitsRect(cv::UMat frame) {
    // Checking that digitsRect is inside the frame.
    cv::Rect frameRect({0, 0}, frame.size());
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

    if (whiteColor < 175 || maxBrightness < 100) {
        // The image is too dark. In Sonic games transitions to black are happening after the timer has stopped anyways.
        return {};
    }

    // We only need to recognize time before transition to white on Sonic 2's Death Egg.
    bool recognizeWhiteTransition = (gameName == "Sonic 2" && frameAnalyzer.getCurrentSplitIndex() == 19);
    
    if (!recognizeWhiteTransition) {
        uint8_t maxAcceptableBrightness;
        if (gameName == "Sonic 2") {
            // Sonic 2's Chemical Plant is white enough, so we accept any brightness.
            maxAcceptableBrightness = 255;
        }
        else if (gameName == "Sonic CD" && frameAnalyzer.getCurrentSplitIndex() == 20) {
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
    return gameName == "Sonic CD";
}


std::tuple<cv::UMat, cv::UMat, int> TimeRecognizer::loadImageAndMaskFromFile(char symbol) {
    // The path can contain unicode, so we read the file by ourselves.
    std::filesystem::path templatePath = templatesDirectory / (symbol + std::string(".png"));
    std::ifstream fin(templatePath, std::ios::binary);
    std::vector<uint8_t> templateBuffer((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    fin.close();

    cv::UMat templateWithAlpha;
    cv::imdecode(templateBuffer, cv::IMREAD_UNCHANGED).copyTo(templateWithAlpha);
    cv::resize(templateWithAlpha, templateWithAlpha, {}, 2, 2, cv::INTER_NEAREST);
    // We double the size of the image, as then matching is more accurate.
    
    bool filterYellowColor = (symbol == TIME || symbol == SCORE);
    cv::UMat templateImage = convertFrameToGray(templateWithAlpha, filterYellowColor);

    std::vector<cv::UMat> templateChannels(4);
    cv::split(templateWithAlpha, templateChannels);

    cv::UMat templateMask = templateChannels[3];  // Get the alpha channel.
    cv::threshold(templateMask, templateMask, 0, 1.0, cv::THRESH_BINARY);
    templateMask.convertTo(templateMask, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.

    int opaquePixels = cv::countNonZero(templateMask);

    return {templateImage, templateMask, opaquePixels};
}


bool TimeRecognizer::Match::operator==(const Match& other) const = default;

}