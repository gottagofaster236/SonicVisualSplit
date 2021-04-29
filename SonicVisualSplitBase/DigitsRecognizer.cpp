﻿#include "DigitsRecognizer.h"
#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <cmath>


namespace SonicVisualSplitBase {

DigitsRecognizer& DigitsRecognizer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite) {
    if (!instance || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory) {
        instance = std::unique_ptr<DigitsRecognizer>(new DigitsRecognizer(gameName, templatesDirectory, isComposite));
    }
    return *instance;
}


std::vector<DigitsRecognizer::Match> DigitsRecognizer::findLabelsAndDigits(cv::UMat frame, bool checkForScoreScreen) {
    if (shouldResetDigitsPlacement) {
        shouldResetDigitsPlacement = false;
        resetDigitsPlacement();
    }

    // Template matching for TIME and SCORE is done on the grayscale frame where yellow is white (it's more accurate this way).
    cv::UMat frameWithYellowFilter;

    if (bestScale != -1 && !digitsRect.empty()) {
        cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
        if (checkForScoreScreen) {
            frameWithYellowFilter = convertFrameToGray(frame, true);
        }
        else {
            // We are looking for digits only, so cropping the frame do the rectangle where digits are located.
            frame = cropToDigitsRectAndCorrectColor(frame);
            if (frame.empty())
                return {};
        }
    }
    else {
        /* We need to find the digits rectangle. It is relative to the TIME label, so we need to find it.
         * Since we check for score screen by finding the positions of TIME label (as in TIME BONUS), we'll do exactly that. */
        checkForScoreScreen = true;
        frameWithYellowFilter = convertFrameToGray(frame, true);
    }

    if (checkForScoreScreen) {
        // We're gonna recalculate digits rectangle when we're checking for score screen.
        digitsRect = {0, 0, 0, 0};
    }

    std::vector<Match> allMatches;

    std::vector<char> symbolsToSearch;
    if (checkForScoreScreen) {
        symbolsToSearch.push_back(TIME);
        symbolsToSearch.push_back(SCORE);
    }
    for (int i = 0; i <= 9; i++)
        symbolsToSearch.push_back('0' + i);

    for (char symbol : symbolsToSearch) {
        const cv::UMat& frameToSearch = ((symbol == TIME || symbol == SCORE) ? frameWithYellowFilter : frame);
        bool recalculateDigitsPlacement = (bestScale == -1);
        recalculatedDigitsPlacement = recalculatedDigitsPlacement;
        std::vector<Match> symbolMatches = findSymbolLocations(frameToSearch, symbol, recalculateDigitsPlacement);

        for (auto& match : symbolMatches) {
            match.similarity *= getSymbolSimilarityMultiplier(symbol);
            // We've cropped the frame to speed up the search. Now we have to compensate for that.
            match.location += cv::Point2f((float) (digitsRect.x / bestScale), (float) (digitsRect.y / bestScale));
            allMatches.push_back(match);
        }

        if (symbol == TIME) {
            if (symbolMatches.empty())
                return {};
            // Checking only the top half of the frame, so that "SCORE" is searched faster.
            cv::Rect topHalf = {0, 0, frame.cols, frame.rows / 2};
            frameWithYellowFilter = frameWithYellowFilter(topHalf);
            if (recalculateDigitsPlacement) {
                // bestScale is calculated already after we found "TIME".
                cv::resize(frameWithYellowFilter, frameWithYellowFilter, cv::Size(),
                    bestScale, bestScale, cv::INTER_AREA);
                cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
            }
        }
        else if (symbol == SCORE) {
            /* We've searched for "TIME" and "SCORE". (Searching "SCORE" so that it's not confused with "TIME".)
             * We'll look for digits only in the rectangle to the right of "TIME".
             * That's why we'll crop the frame to that rectangle. */
            if (symbolMatches.empty())
                return {};
            std::vector<Match> curRecognized = allMatches;
            removeOverlappingMatches(curRecognized);
            cv::Rect2f timeRect = {1e9, 1e9, 1e9, 1e9};
            for (const auto& match : curRecognized) {
                if (match.symbol == TIME && match.location.y < timeRect.y)
                    timeRect = match.location;
            }

            double rightBorderCoefficient = (gameName == "Sonic CD" ? 3.2 : 2.55);
            int digitsRectLeft = (int) ((timeRect.x + timeRect.width * 1.25) * bestScale);
            int digitsRectRight = (int) ((timeRect.x + timeRect.width * rightBorderCoefficient) * bestScale);
            int digitsRectTop = (int) ((timeRect.y - timeRect.height * 0.1) * bestScale);
            int digitsRectBottom = (int) ((timeRect.y + timeRect.height * 1.1) * bestScale);
            digitsRectLeft = std::max(digitsRectLeft, 0);
            digitsRectRight = std::min(digitsRectRight, frame.cols);
            digitsRectTop = std::max(digitsRectTop, 0);
            digitsRectBottom = std::min(digitsRectBottom, frame.rows);
            digitsRect = {digitsRectLeft, digitsRectTop, digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
            if (digitsRect.empty())
                return {};
            int oldWidth = frame.cols, oldHeight = frame.rows * 2 + 1;
            frame = cropToDigitsRectAndCorrectColor(frame);
            if (frame.empty())
                return curRecognized;

            relativeDigitsRect = {(float) digitsRect.x / oldWidth, (float) digitsRect.y / oldHeight,
                (float) digitsRect.width / oldWidth, (float) digitsRect.height / oldHeight};
        }
    }
    
    removeMatchesWithLowSimilarity(digitsMatches);  // nope!
    /* Will have two arrays for matches: "labelMatches" and "digitMatches".
     * Call removeLow after we've found SCORE and here.
    */
    0 = 0;
    // merge the matches here into allMatches (and define the variable itself here, not higher).
    removeMatchesWithIncorrectYCoord(allMatches);
    removeOverlappingMatches(allMatches);
    return allMatches;
}


void DigitsRecognizer::resetDigitsPlacement() {
    FrameAnalyzer::lockFrameAnalyzationMutex();
    if (instance) {
        instance->bestScale = -1;
        instance->digitsRect = {0, 0, 0, 0};
    }
    FrameAnalyzer::unlockFrameAnalyzationMutex();
}


void DigitsRecognizer::resetDigitsPlacementAsync() {
    shouldResetDigitsPlacement = true;
}


bool DigitsRecognizer::recalculatedDigitsPlacementLastTime() const {
    return recalculatedDigitsPlacement;
}


double DigitsRecognizer::getBestScale() const {
    return bestScale;
}


const std::unique_ptr<DigitsRecognizer>& DigitsRecognizer::getCurrentInstance() {
    return instance;
}


cv::Rect2f DigitsRecognizer::getRelativeDigitsRect() {
    return relativeDigitsRect;
}


DigitsRecognizer::DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite)
        : gameName(gameName), templatesDirectory(templatesDirectory), isComposite(isComposite) {
    std::vector<char> symbolsToLoad = {TIME, SCORE};
    for (char digit = '0'; digit <= '9'; digit++)
        symbolsToLoad.push_back(digit);
    for (char symbol : symbolsToLoad) {
        templates[symbol] = loadImageAndMaskFromFile(symbol);
    }
}


/* Returns all found positions of a digit. If recalculateDigitsPlacement is set to true,
 * goes through different scales of the original frame (see the link below).
 * Algorithm based on: https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv */
std::vector<DigitsRecognizer::Match> DigitsRecognizer::findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement) {
    const auto& [templateImage, templateMask, opaquePixels] = templates[symbol];

    // if we already found the scale, we don't go through different scales
    double minScale, maxScale;

    if (recalculateDigitsPlacement) {
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

    const double MIN_SIMILARITY = -3500;
    double bestSimilarityForScales = MIN_SIMILARITY;  // Used only if recalculateDigitsPlacement is true.

    std::vector<Match> matches;  // Pairs: {supposed digit location, similarity coefficient}.

    for (double scale = maxScale; scale >= minScale; scale *= 0.96) {
        cv::UMat resized;
        if (scale != 1)
            cv::resize(frame, resized, cv::Size(), scale, scale, cv::INTER_AREA);
        else
            resized = frame;

        if (resized.cols < templateImage.cols || resized.rows < templateImage.rows)
            continue;
        cv::UMat matchResult;
        cv::matchTemplate(resized, templateImage, matchResult, cv::TM_SQDIFF, templateMask);

        if (recalculateDigitsPlacement) {
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
        cv::threshold(matchResult, matchResultBinary, MIN_SIMILARITY, 1, cv::THRESH_BINARY_INV);
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
                                 (float) (templateImage.cols / bestScale),
                                 (float) (templateImage.rows / bestScale));
            matches.push_back({matchRect, symbol, similarity});
        }
    }

    if (matches.size() < 1000)
        return matches;
    else  // There cannot be that many matches on a correct image. (It's probably a single color image).
        return {};
}


void DigitsRecognizer::removeMatchesWithLowSimilarity(std::vector<Match>& matches) {
    double bestSimilarity = std::ranges::max(
        std::views::transform(matches, &Match::similarity));
    std::erase_if(matches, [&](const Match& match) {
        double similarityCoefficient = getSymbolMinSimilarityCoefficient(match.symbol);
        double similarityMultiplier = getSymbolSimilarityMultiplier(match.symbol);
        double minSimilarity = bestSimilarity * similarityCoefficient * similarityMultiplier;
        return match.similarity < minSimilarity;
    });
}


void DigitsRecognizer::removeOverlappingMatches(std::vector<Match>& matches) {
    // Sorting the matches by similarity in descending order, and removing the overlapping ones.
    sortMatchesBySimilarity(matches);
    std::vector<Match> resultSymbolLocations;

    for (const auto& match : matches) {
        bool intersectsWithOthers = false;

        for (const auto& digit : resultSymbolLocations) {
            const cv::Rect2f& other = digit.location;

            if (std::isdigit(match.symbol) && std::isdigit(digit.symbol)) {
                if (std::abs(match.location.x + match.location.width - (other.x + other.width)) * bestScale < 12) {
                    intersectsWithOthers = true;
                }
            }
            else if (!(match.location & other).empty()) {
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


void DigitsRecognizer::removeMatchesWithIncorrectYCoord(std::vector<Match>& matches) {
    sortMatchesBySimilarity(matches);
    auto digitMatches = matches | std::views::filter([](const Match& match) {
        return std::isdigit(match.symbol);
    });
    if (digitMatches.empty())
        return;
    const cv::Rect2f& bestMatchLocation = digitMatches.begin()->location;

    std::erase_if(matches, [&](const Match& match) {
        if (!std::isdigit(match.symbol))
            return false;
        // If the difference in the y coordinates is more than one pixel, we consider it a wrong match.
        return std::abs(match.location.y - bestMatchLocation.y) * bestScale > 1.5;
    });
}


void DigitsRecognizer::sortMatchesBySimilarity(std::vector<DigitsRecognizer::Match>& matches) {
    std::ranges::sort(matches, std::greater<>(), &Match::similarity);
}


double DigitsRecognizer::getSymbolMinSimilarityCoefficient(char symbol) {
    switch (symbol) {
    default:
        return 3.75;
    case SCORE:
        return 2;
    // We use "TIME" to detect the score screen, so we want to be sure.
    case TIME:
        if (isComposite)
            return 2;
        else
            return 1.5;
    /* One is really small, so it can be misdetected, thus the coefficient is lowered.
     * This leads to four recognizing instead of one - so coefficient for four is lowered too. */
    case '1':
        if (isComposite)
            return 2.15;
        else
            return 2.5;
    case '4':
        return 2;
    }
}


double DigitsRecognizer::getSymbolSimilarityMultiplier(char symbol) {
    switch (symbol)
    {
    // No multiplier by default.
    default:
        return 1;
    // Once again making one a less preferable option.
    case '1':
        return 2;
    // Seven is confused with one and two.
    case '7':
        return 1.2;
    // Nine is confused with eight.
    case '9':
        return 1.1;
    }
}


cv::UMat DigitsRecognizer::cropToDigitsRectAndCorrectColor(cv::UMat frame) {
    frame = frame(digitsRect);
    frame = convertFrameToGray(frame);
    frame = applyColorCorrection(frame);
    return frame;
}


cv::UMat DigitsRecognizer::applyColorCorrection(cv::UMat img) {
    std::vector<uint8_t> pixels;
    cv::Mat imgMat = img.getMat(cv::ACCESS_READ);
    for (int y = 0; y < img.rows; y += 2) {
        for (int x = 0; x < img.cols; x += 2) {
            pixels.push_back((uint8_t) imgMat.at<float>(y, x));
        }
    }
    std::ranges::sort(pixels);
    
    const float darkPosition = 0.25f, brightPosition = 0.8f;
    uint8_t minBrightness = pixels[(int) (pixels.size() * darkPosition)];
    uint8_t maxBrightness = pixels[(int) (pixels.size() * brightPosition)];
    uint8_t difference = maxBrightness - minBrightness;
    const uint8_t MIN_DIFFERENCE = 4;

    if (difference < MIN_DIFFERENCE) {
        // This is an almost single-color image, there's no point in increasing the contrast.
        return cv::UMat();
    }
    else if (maxBrightness < 150) {
        // The image is too dark. In Sonic games transitions to black are happening after the timer has stopped anyways.
        return cv::UMat();
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


cv::UMat DigitsRecognizer::convertFrameToGray(cv::UMat frame, bool filterYellowColor) {
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


std::tuple<cv::UMat, cv::UMat, int> DigitsRecognizer::loadImageAndMaskFromFile(char symbol) {
    // The path can contain unicode, so we read the file by ourselves.
    std::filesystem::path templatePath = templatesDirectory / (symbol + std::string(".png"));
    std::ifstream fin(templatePath, std::ios::binary);
    std::vector<uint8_t> templateBuffer((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    fin.close();

    cv::UMat templateWithAlpha;
    cv::imdecode(templateBuffer, cv::IMREAD_UNCHANGED).copyTo(templateWithAlpha);
    cv::resize(templateWithAlpha, templateWithAlpha, cv::Size(), 2, 2, cv::INTER_NEAREST);
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

}