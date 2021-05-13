#include "DigitsRecognizer.h"
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
        resetDigitsPlacement();
        shouldResetDigitsPlacement = false;
    }

    if (frame.size() != lastFrameSize) {
        resetDigitsPlacement();
        lastFrameSize = frame.size();
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

    std::vector<char> symbolsToSearch;
    if (checkForScoreScreen) {
        symbolsToSearch.push_back(TIME);
        symbolsToSearch.push_back(SCORE);
    }
    for (int i = 0; i <= 9; i++)
        symbolsToSearch.push_back('0' + i);

    std::vector<Match> labelMatches;  // "TIME" and "SCORE" labels.
    std::vector<Match> digitMatches;  // Time digits.

    for (char symbol : symbolsToSearch) {
        const cv::UMat& frameToSearch = (std::isdigit(symbol) ? frame : frameWithYellowFilter);
        bool recalculateDigitsPlacement = (bestScale == -1);
        recalculatedDigitsPlacementLastTime = recalculateDigitsPlacement;
        std::vector<Match> symbolMatches = findSymbolLocations(frameToSearch, symbol, recalculateDigitsPlacement);

        for (auto& match : symbolMatches) {
            match.similarity *= getSymbolSimilarityMultiplier(symbol);
            // We've cropped the frame to speed up the search. Now we have to compensate for that.
            match.location += cv::Point2f((float) (digitsRect.x / bestScale), (float) (digitsRect.y / bestScale));
            std::vector<Match>& matches = (std::isdigit(symbol) ? digitMatches : labelMatches);
            matches.push_back(match);
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
            removeMatchesWithLowSimilarity(labelMatches);
            removeOverlappingMatches(labelMatches);
            if (labelMatches.size() > 15) {
                // There can't be that many matches, even if some of them are wrong.
                return {};
            }

            cv::Rect2f timeRect = {1e9, 1e9, 1e9, 1e9};
            for (const auto& match : labelMatches) {
                if (match.symbol == TIME && match.location.y < timeRect.y)
                    timeRect = match.location;
            }

            double rightBorderCoefficient = (gameName == "Sonic CD" ? 3.2 : 2.45);
            int digitsRectLeft = (int) ((timeRect.x + timeRect.width * 1.22) * bestScale);
            int digitsRectRight = (int) ((timeRect.x + timeRect.width * rightBorderCoefficient) * bestScale);
            int digitsRectTop = (int) ((timeRect.y - timeRect.height * 0.1) * bestScale);
            int digitsRectBottom = (int) ((timeRect.y + timeRect.height * 1.1) * bestScale);
            digitsRect = {digitsRectLeft, digitsRectTop, digitsRectRight - digitsRectLeft, digitsRectBottom - digitsRectTop};
            if (digitsRect.empty())
                return {};
            frame = cropToDigitsRectAndCorrectColor(frame);
            if (frame.empty())
                return labelMatches;
        }
    }
    
    removeMatchesWithLowSimilarity(digitMatches);
    removeMatchesWithIncorrectYCoord(digitMatches);
    removeOverlappingMatches(digitMatches);

    std::vector<Match> allMatches = std::move(digitMatches);
    allMatches.insert(allMatches.end(), labelMatches.begin(), labelMatches.end());
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


void DigitsRecognizer::fullReset() {
    FrameAnalyzer::lockFrameAnalyzationMutex();
    resetDigitsPlacement();
    if (instance)
        instance->relativeDigitsRect = {};
    FrameAnalyzer::unlockFrameAnalyzationMutex();
}


double DigitsRecognizer::getBestScale() const {
    return bestScale;
}


void DigitsRecognizer::reportRecognitionSuccess() {
    float frameWidth = (float) (lastFrameSize.width * bestScale);
    float frameHeight = (float) (lastFrameSize.height * bestScale);
    relativeDigitsRect = {digitsRect.x / frameWidth, digitsRect.y / frameHeight,
        digitsRect.width / frameWidth, digitsRect.height / frameHeight};
    relativeDigitsRect &= cv::Rect2f(0, 0, 1, 1);  // Make sure the it isn't out of bounds.
    relativeDigitsRectUpdatedTime = std::chrono::system_clock::now();
}


void DigitsRecognizer::reportRecognitionFailure() {
    if (recalculatedDigitsPlacementLastTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        resetDigitsPlacement();
    }
}


cv::Rect2f DigitsRecognizer::getRelativeDigitsRect() {
    auto currentTime = std::chrono::system_clock::now();
    if (currentTime - relativeDigitsRectUpdatedTime < std::chrono::seconds(10)) {
        return relativeDigitsRect;
    }
    else {
        // We consider the relative digits rectangle outdated now, it hasn't been updated for too long.
        return {};
    }
}


const std::unique_ptr<DigitsRecognizer>& DigitsRecognizer::getCurrentInstance() {
    return instance;
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


static const double MIN_SIMILARITY = -8000;

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

    double bestSimilarityForScales = MIN_SIMILARITY;
    // bestSimilarityForScales is used only if recalculateDigitsPlacement is true.

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
        cv::threshold(matchResult, matchResultBinary, -MIN_SIMILARITY * opaquePixels, 1, cv::THRESH_BINARY_INV);
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

    return matches;
}


void DigitsRecognizer::removeMatchesWithLowSimilarity(std::vector<Match>& matches) {
    if (matches.size() < 2)
        return;
    // Finding the second largest similarity (not the first for precision).
    std::ranges::nth_element(matches, matches.begin() + 1, std::greater<>(), &Match::similarity);
    double bestSimilarity = matches[1].similarity;
    
    std::erase_if(matches, [&](const Match& match) {
        double similarityCoefficient = getSymbolMinSimilarityCoefficient(match.symbol);
        double similarityMultiplier = getSymbolSimilarityMultiplier(match.symbol);
        double minSimilarity = bestSimilarity * similarityCoefficient;
        minSimilarity = std::max(minSimilarity, MIN_SIMILARITY);
        minSimilarity = std::min(minSimilarity, MIN_SIMILARITY / 10);
        return match.similarity / similarityMultiplier < minSimilarity;
    });
}


void DigitsRecognizer::removeOverlappingMatches(std::vector<Match>& matches) {
    // Sorting the matches by similarity in descending order, and removing the overlapping ones.
    std::ranges::sort(matches, std::greater<>(), &Match::similarity);
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


void DigitsRecognizer::removeMatchesWithIncorrectYCoord(std::vector<Match>& digitMatches) {
    if (digitMatches.empty())
        return;
    const Match& bestMatch = std::ranges::max(digitMatches, {}, &Match::similarity);
    const cv::Rect2f& bestMatchLocation = bestMatch.location;

    std::erase_if(digitMatches, [&](const Match& match) {
        // If the difference in the y coordinates is more than one pixel, we consider it a wrong match.
        return std::abs(match.location.y - bestMatchLocation.y) * bestScale > 1.5;
    });
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
            return 1.75;
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
    // Four is confused with one.
    case '4':
        return 1.2;
    // Seven is confused with one and two.
    case '7':
        return 1.15;
    }
}


cv::UMat DigitsRecognizer::cropToDigitsRectAndCorrectColor(cv::UMat frame) {
    // Checking that digitsRect is inside the frame.
    cv::Rect frameRect(0, 0, frame.cols, frame.rows);
    if ((digitsRect & frameRect) != digitsRect) {
        return {};
    }
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
    
    const float darkPosition = 0.25f, brightPosition = 0.85f;
    uint8_t minBrightness = pixels[(int) (pixels.size() * darkPosition)];
    uint8_t maxBrightness = pixels[(int) (pixels.size() * brightPosition)];

    // We only need to recognize time before transition to white on Sonic 2's Death Egg.
    bool recognizeWhiteTransition = (gameName == "Sonic 2" && FrameAnalyzer::getCurrentSplitIndex() == 19);
    
    if (!recognizeWhiteTransition) {
        if (minBrightness > 180 && gameName != "Sonic 2")
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
    else if (maxBrightness < 100) {
        // The image is too dark. In Sonic games transitions to black are happening after the timer has stopped anyways.
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


bool DigitsRecognizer::Match::operator==(const Match& other) const = default;

}