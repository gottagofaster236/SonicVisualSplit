﻿#include "DigitsRecognizer.h"
#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>


namespace SonicVisualSplitBase {

DigitsRecognizer& DigitsRecognizer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isComposite) {
    if (instance == nullptr || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory) {
        delete instance;
        instance = new DigitsRecognizer(gameName, templatesDirectory, isComposite);
    }
    return *instance;
}


std::vector<std::pair<cv::Rect2f, char>> DigitsRecognizer::findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen) {
    if (bestScale != -1 && !digitsRect.empty()) {
        cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
        if (!checkForScoreScreen) {
            // We are looking for digits only, so cropping the frame do the rectangle where digits are located.
            frame = cropToDigitsRectAndCorrectColor(frame);
        }
    }
    else {
        /* We need to find the digits rectangle. It is relative to the TIME label, so we need to find it.
         * Since we check for score screen by finding the positions of TIME label (as in TIME BONUS), we'll do exactly that. */
        checkForScoreScreen = true;
    }

    if (checkForScoreScreen) {
        // We're gonna recalculate digits rectangle when we're checking for score screen.
        digitsRect = {0, 0, 0, 0};
    }

    std::vector<std::tuple<cv::Rect2f, char, double>> digitLocations;  // {location, digit, similarity coefficient}

    std::vector<char> symbolsToSearch;
    if (checkForScoreScreen) {
        symbolsToSearch.push_back(TIME);
        symbolsToSearch.push_back(SCORE);
    }
    for (int i = 0; i <= 9; i++)
        symbolsToSearch.push_back('0' + i);

    for (char symbol : symbolsToSearch) {
        recalculateDigitsPlacement = (bestScale == -1);
        std::vector<std::pair<cv::Rect2f, double>> matches = findSymbolLocations(frame, symbol, recalculateDigitsPlacement);

        for (auto [location, similarity] : matches) {
            similarity *= getSymbolSimilarityMultiplier(symbol);
            // We've cropped the frame to speed up the search. Now we have to compensate for that.
            location += cv::Point2f((float) (digitsRect.x / bestScale), (float) (digitsRect.y / bestScale));
            digitLocations.push_back({location, symbol, similarity});
        }

        if (symbol == TIME) {
            if (matches.empty())  // Cannot find the "TIME" label, but it should be there on a correct frame.
                return {};
            // Checking only the top half of the frame, so that "SCORE" is searched faster.
            cv::Rect topHalf = {0, 0, frame.cols, frame.rows / 2};
            frame = frame(topHalf);
            if (recalculateDigitsPlacement) {
                // bestScale is calculated already after we found "TIME".
                cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
            }
        }
        else if (symbol == SCORE) {
            /* We've searched for "TIME" and "SCORE". (Searching "SCORE" so that it's not confused with "TIME".)
             * We'll look for digits only in the rectangle to the right of "TIME".
             * That's why we'll crop the frame to that rectangle. */
            if (matches.empty())
                return {};
            std::vector<std::pair<cv::Rect2f, char>> curRecognized = removeOverlappingLocations(digitLocations);
            cv::Rect2f timeRect = {1e9, 1e9, 1e9, 1e9};
            for (const auto& [location, symbol] : curRecognized) {
                if (symbol == TIME && location.y < timeRect.y)
                    timeRect = location;
            }

            double rightBorderCoefficient = (gameName == "Sonic CD" ? 3.3 : 2.55);
            int digitsRectLeft = (int) ((timeRect.x + timeRect.width * 1.25) * bestScale);
            int digitsRectRight = (int) ((timeRect.x + timeRect.width * rightBorderCoefficient) * bestScale);
            int digitsRectTop = (int) ((timeRect.y - timeRect.height * 0.2) * bestScale);
            int digitsRectBottom = (int) ((timeRect.y + timeRect.height * 1.2) * bestScale);
            digitsRectLeft = std::max(digitsRectLeft, 0);
            digitsRectRight = std::min(digitsRectRight, frame.cols);
            digitsRectTop = std::max(digitsRectTop, 0);
            digitsRectBottom = std::min(digitsRectBottom, frame.rows);
            digitsRect = {digitsRectLeft, digitsRectTop, digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
            if (digitsRect.empty())
                return {};
            int oldWidth = frame.cols, oldHeight = frame.rows * 2 + 1;
            frame = cropToDigitsRectAndCorrectColor(frame);

            relativeDigitsRect = {(float) digitsRect.x / oldWidth, (float) digitsRect.y / oldHeight,
                (float) digitsRect.width / oldWidth, (float) digitsRect.height / oldHeight};
        }
    }

    return removeOverlappingLocations(digitLocations);
}


cv::UMat DigitsRecognizer::convertFrameToGray(cv::UMat frame) {
    /* Getting the yellow color intensity by adding green and red channels (BGR).
     * TIME and SCORE are yellow, so it's probably a good way to convert a frame to grayscale. 
     * (In the first place it is done to speed up the template matching by reducing the number of channels to 1). */
    std::vector<cv::UMat> channels(4);
    cv::split(frame, channels);
    cv::UMat result;
    cv::addWeighted(channels[1], 0.5, channels[2], 0.5, 0, result);
    return result;
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
    // Not waiting for digitsRecognizerMutex.
    std::thread(resetDigitsPlacement).detach();
}

bool DigitsRecognizer::recalculatedDigitsPlacementLastTime() {
    return recalculateDigitsPlacement;
}


double DigitsRecognizer::getBestScale() {
    return bestScale;
}


DigitsRecognizer* DigitsRecognizer::getCurrentInstance() {
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
std::vector<std::pair<cv::Rect2f, double>> DigitsRecognizer::findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement) {
    auto& [templateImage, templateMask, opaquePixels] = templates[symbol];

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

    if (recalculateDigitsPlacement) {
        const double MIN_SIMILARITY = -13000;
        bestSimilarity = MIN_SIMILARITY / getSymbolMinSimilarityCoefficient('0');
    }

    std::vector<std::pair<cv::Rect2f, double>> matches;  // Pairs: {supposed digit location, similarity coefficient}.

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
            if (bestSimilarity < bestSimilarityForScale) {
                bestSimilarity = bestSimilarityForScale;
                bestScale = scale;
            }
            else {
                // This is not the best scale.
                continue;
            }
        }

        matches.clear();

        double similarityCoefficient = getSymbolMinSimilarityCoefficient(symbol);
        double maximumAcceptableSqdiff = -similarityCoefficient * bestSimilarity * opaquePixels;
        if (maximumAcceptableSqdiff == 0)  // Someone is testing this on an emulator, so perfect matches are possible.
            maximumAcceptableSqdiff = -bestSimilarity / 10 * opaquePixels;

        cv::UMat matchResultBinary;
        cv::threshold(matchResult, matchResultBinary, maximumAcceptableSqdiff, 1, cv::THRESH_BINARY_INV);
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
            matches.push_back({matchRect, similarity});
        }
    }

    if (matches.size() < 1000)
        return matches;
    else  // There cannot be that many matches on a correct image. (It's probably a single color image).
        return {};
}


std::vector<std::pair<cv::Rect2f, char>> DigitsRecognizer::removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations) {
    // Sorting the matches by similarity in descending order, and removing the overlapping ones.
    std::vector<std::pair<cv::Rect2f, char>> resultDigitLocations;

    std::sort(digitLocations.begin(), digitLocations.end(), [this](const auto& lhs, const auto& rhs) {
        return std::get<2>(lhs) > std::get<2>(rhs);
    });
    for (const auto& [location, symbol, similarity] : digitLocations) {
        bool intersectsWithOthers = false;

        for (auto& digit : resultDigitLocations) {
            const cv::Rect2f& other = digit.first;

            if (std::isdigit(symbol) && std::isdigit(digit.second)) {
                if (std::abs(location.x + location.width - (other.x + other.width)) * bestScale < 12) {
                    intersectsWithOthers = true;
                }
            }
            else {
                if (!(location & other).empty()) {
                    intersectsWithOthers = true;
                }
            }

            if (intersectsWithOthers)
                break;
        }

        if (!intersectsWithOthers)
            resultDigitLocations.push_back({location, symbol});
    }

    return resultDigitLocations;
}


double DigitsRecognizer::getSymbolMinSimilarityCoefficient(char symbol) {
    switch (symbol) {
    default:
        return 3.25;
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
            return 2;
        else
            return 2.5;
    case '4':
        return 2.5;
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
    // Three is often confused with eight, make it more preferable.
    case '3':
        return 0.8;
    }
}


cv::UMat DigitsRecognizer::cropToDigitsRectAndCorrectColor(cv::UMat frame) {
    frame = frame(digitsRect);
    applyColorCorrection(frame);
    return frame;
}


void DigitsRecognizer::applyColorCorrection(cv::UMat img) {
    double minBrightness, maxBrightness;
    cv::minMaxLoc(img, &minBrightness, &maxBrightness);
    float difference = (float) (maxBrightness - minBrightness);

    // Making the minimum brightness equal to 0 and the maximum brightness equal to 255.
    cv::subtract(img, cv::Scalar((float) minBrightness), img);
    if (difference != 0) {
        // Make sure we don't divide by zero.
        cv::multiply(img, cv::Scalar(255 / difference), img);
    }
}


// Separates the image and its alpha channel.
// Returns a tuple of {image, binary alpha mask, count of opaque pixels}
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
    
    cv::UMat templateImage = convertFrameToGray(templateWithAlpha);
    templateImage.convertTo(templateImage, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.

    std::vector<cv::UMat> templateChannels(4);
    cv::split(templateWithAlpha, templateChannels);

    cv::UMat templateMask = templateChannels[3];  // Get the alpha channel.
    cv::threshold(templateMask, templateMask, 0, 1.0, cv::THRESH_BINARY);
    templateMask.convertTo(templateMask, CV_32F);

    int opaquePixels = cv::countNonZero(templateMask);

    return {templateImage, templateMask, opaquePixels};
}

}