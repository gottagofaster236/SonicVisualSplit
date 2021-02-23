#include "DigitsRecognizer.h"
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


// Find locations of all digits, "SCORE" and "TIME" labels.
std::vector<std::pair<cv::Rect2f, char>> DigitsRecognizer::findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen) {
    if (bestScale != -1 && !digitsRoi.empty()) {
        cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
        if (!checkForScoreScreen)  // if we look for digits only, we can speed everything up
            frame = frame(digitsRoi);
    }
    else { // we need to find the TIME label to calculate the digits ROI
        checkForScoreScreen = true;
    }

    if (checkForScoreScreen)
        digitsRoi = {0, 0, 0, 0};  // we're gonna recalculate it
    // ROI stands for region of interest.

    std::vector<std::tuple<cv::Rect2f, char, double>> digitLocations;   // {location, digit, similarity coefficient}

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
            // we've changed the ROI to speed up the search. Now we have to compensate for that.
            location += cv::Point2f((float) (digitsRoi.x / bestScale), (float) (digitsRoi.y / bestScale));
            digitLocations.push_back({location, symbol, similarity});
        }

        if (symbol == TIME) {
            if (matches.empty())  // cannot find the "TIME" label - but it should be there!
                return {};
            /* We search the whole screen for the "TIME" label.
             * For other symbols we will speed up the calculation by scaling the image down to the best scale. */
            cv::Rect topHalf = {0, 0, frame.cols, frame.rows / 2};
            frame = frame(topHalf);  // so that "SCORE" is searched faster
            if (recalculateDigitsPlacement) {
                cv::resize(frame, frame, cv::Size(), bestScale, bestScale, cv::INTER_AREA);
            }
        }
        else if (symbol == SCORE) {
            /* We've searched for "TIME" and "SCORE". (Searching "SCORE" so that it's not confused with "TIME".)
             * We'll look for the digits only in the horizontal stripe containing the word "TIME".
             * To "crop" the frame to that horizontal stripe, we simply change the ROI (region of interest). */
            if (matches.empty())
                return {};
            std::vector<std::pair<cv::Rect2f, char>> curRecognized = removeOverlappingLocations(digitLocations);
            cv::Rect2f timeRect = {1e9, 1e9, 1e9, 1e9};
            for (const auto& [location, symbol] : curRecognized) {
                if (symbol == TIME && location.y < timeRect.y)
                    timeRect = location;
            }

            double rightBorderCoefficient = (gameName == "Sonic CD" ? 3.3 : 2.55);
            int roiLeft = (int) ((timeRect.x + timeRect.width * 1.28) * bestScale);
            int roiRight = (int) ((timeRect.x + timeRect.width * rightBorderCoefficient) * bestScale);
            int roiTop = (int) ((timeRect.y - timeRect.height * 0.2) * bestScale);
            int roiBottom = (int) ((timeRect.y + timeRect.height * 1.2) * bestScale);
            roiLeft = std::max(roiLeft, 0);
            roiRight = std::min(roiRight, frame.cols);
            roiTop = std::max(roiTop, 0);
            roiBottom = std::min(roiBottom, frame.rows);
            digitsRoi = {roiLeft, roiTop, roiRight - roiLeft, roiBottom - roiTop};
            if (digitsRoi.empty())
                return {};
            int oldWidth = frame.cols, oldHeight = frame.rows * 2 + 1;
            frame = frame(digitsRoi);

            relativeDigitsRoi = {(float) digitsRoi.x / oldWidth, (float) digitsRoi.y / oldHeight,
                (float) digitsRoi.width / oldWidth, (float) digitsRoi.height / oldHeight};
        }
    }

    return removeOverlappingLocations(digitLocations);
}


void DigitsRecognizer::resetDigitsPlacement() {
    FrameAnalyzer::lockFrameAnalyzationMutex();
    if (instance) {
        instance->bestScale = -1;
        instance->digitsRoi = {0, 0, 0, 0};
    }
    FrameAnalyzer::unlockFrameAnalyzationMutex();
}

void DigitsRecognizer::resetDigitsPlacementAsync() {
    // Not waiting for digitsRecognizerMutex.
    std::thread(DigitsRecognizer::resetDigitsPlacement).detach();
}

bool DigitsRecognizer::recalculatedDigitsPlacementLastTime() {
    return recalculateDigitsPlacement;
}


DigitsRecognizer* DigitsRecognizer::getCurrentInstance() {
    return instance;
}


cv::Rect2f DigitsRecognizer::getRelativeDigitsRoi() {
    return relativeDigitsRoi;
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


// Returns all found positions of a digit. If recalculateDigitsPlacement is set to true, goes through different scales of the original frame (see the link below).
// Algorithm based on: https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv
std::vector<std::pair<cv::Rect2f, double>> DigitsRecognizer::findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement) {
    auto& [templateImage, templateMask, opaquePixels] = templates[symbol];

    // if we already found the scale, we don't go through different scales
    double minScale, maxScale;

    if (recalculateDigitsPlacement) {
        // Sega Genesis resolution is 320×224 (height = 224)
        // we scale up the template images 2x, so we have to do the same for the resolution.
        const int minHeight = 200 * 2;
        const int maxHeight = 320 * 2;
        // 224 / 320 ≈ 0.75
        // Thus the frame should take at least 75% of stream's height.
        minScale = ((double) minHeight) / frame.rows;
        maxScale = ((double) maxHeight) / frame.rows;
    }
    else {
        minScale = maxScale = 1;
    }

    if (recalculateDigitsPlacement)
        bestSimilarity = MIN_SIMILARITY / SIMILARITY_COEFFICIENT;

    std::vector<std::pair<cv::Rect2f, double>> matches;  // pairs: {supposed digit location, similarity coefficient}

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
            double maxSimilarityForScale = -minimumSqdiff / opaquePixels;
            if (bestSimilarity < maxSimilarityForScale) {
                bestSimilarity = maxSimilarityForScale;
                bestScale = scale;
            }
            else {
                // this is not the best scale
                continue;
            }
        }

        matches.clear();

        double similarityCoefficient;
        switch (symbol) {
        default:
            similarityCoefficient = SIMILARITY_COEFFICIENT;
            break;
        case TIME: case SCORE:
            similarityCoefficient = TIME_SIMILARITY_COEFFICIENT;
            break;
        case '1':
            similarityCoefficient = ONE_SIMILARITY_COEFFICIENT;
            break;
        case '4':
            similarityCoefficient = FOUR_SIMILARITY_COEFFICIENT;
            break;
        }

        double maximumSqdiff = -similarityCoefficient * bestSimilarity * opaquePixels;
        if (maximumSqdiff == 0)  // Someone is testing this on an emulator, so perfect matches are possible.
            maximumSqdiff = -bestSimilarity / 10 * opaquePixels;

        cv::UMat matchResultBinary;
        cv::threshold(matchResult, matchResultBinary, maximumSqdiff, 1, cv::THRESH_BINARY_INV);
        std::vector<cv::Point> matchPoints;
        cv::findNonZero(matchResultBinary, matchPoints);

        cv::Mat matchResultRead = matchResult.getMat(cv::ACCESS_READ);

        for (const cv::Point& match : matchPoints) {
            int x = match.x, y = match.y;
            // If the method of template matching is square difference (TM_SQDIFF), then less is better.
            // We want less to be worse, so we make it negative.
            double similarity = -matchResultRead.at<float>(y, x) / opaquePixels;

            // After the scale brute force, we resize the image to the best scale
            // So the rectangle of match will be different too
            double actualScale;
            if (!recalculateDigitsPlacement)
                actualScale = bestScale;
            else
                actualScale = scale;

            if (symbol == '1')  // We want the right edge to be aligned with the 8x8 tile of the digit.
                x += 2;

            cv::Rect2f matchRect((float) (x / actualScale), (float) (y / actualScale),
                                 (float) (templateImage.cols / actualScale),
                                 (float) (templateImage.rows / actualScale));
            matches.push_back({matchRect, similarity});
        }
    }

    if (matches.size() < 1000)
        return matches;
    else  // There cannot be that many matches on a correct image. (It's probably a single color image).
        return {};
}


std::vector<std::pair<cv::Rect2f, char>> DigitsRecognizer::removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations) {
    // Sort the matches by similarity in descending order, and remove the overlapping ones.
    std::vector<std::pair<cv::Rect2f, char>> resultDigitLocations;

    std::sort(digitLocations.begin(), digitLocations.end(), [this](const auto& lhs, const auto& rhs) {
        return std::get<2>(lhs) > std::get<2>(rhs);
    });
    for (const auto& [location, symbol, similarity] : digitLocations) {
        bool intersectsWithOthers = false;

        for (auto& digit : resultDigitLocations) {
            const cv::Rect2f& other = digit.first;

            if (std::isdigit(symbol) && std::isdigit(digit.second)) {
                if (std::abs(location.x + location.width - (other.x + other.width)) < 14) {
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

    cv::UMat templateImage;
    cv::cvtColor(templateWithAlpha, templateImage, cv::COLOR_BGRA2GRAY);
    templateImage.convertTo(templateImage, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.

    std::vector<cv::UMat> templateChannels(4);
    cv::split(templateWithAlpha, templateChannels);
    cv::UMat templateMask = templateChannels[3];  // get the alpha channel
    cv::threshold(templateMask, templateMask, 0, 1.0, cv::THRESH_BINARY);
    templateMask.convertTo(templateMask, CV_32F);

    int opaquePixels = cv::countNonZero(templateMask);

    return {templateImage, templateMask, opaquePixels};
}

}