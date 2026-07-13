#include "TemplateMatcher.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace SonicVisualSplitBase {

const std::string TemplateMatcher::SCORE = "S";
const std::string TemplateMatcher::TIME = "T";
const std::vector<std::string> TemplateMatcher::DIGIT_TEMPLATES{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
const std::vector<std::string> TemplateMatcher::ALL_TEMPLATES{SCORE, TIME, "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};


TemplateMatcher::TemplateMatcher(const AnalysisSettings& settings, const std::vector<std::string>& templateNames) : settings(settings) {
    for (const std::string& templateName : templateNames) {
        templates[templateName] = loadTemplate(templateName);
    }
}


TemplateMatcher::~TemplateMatcher() = default;


std::vector<TemplateMatcher::Match> TemplateMatcher::findTemplateLocations(
    const cv::UMat& src, const std::vector<std::string>& templateNames, bool matchDigits, int splitIndex) const {
    std::vector<Match> matches;
    cv::UMat graySrc, graySrcFilteredYellowColor;
    for (const std::string& templateName : templateNames) {
        bool filterYellowColor = shouldFilterYellowColor(templateName);
        cv::UMat& correctSrc = filterYellowColor ? graySrcFilteredYellowColor : graySrc;
        if (correctSrc.empty()) {
            correctSrc = convertToGray(src, filterYellowColor);
            if (!filterYellowColor && matchDigits) {
                correctSrc = applyColorCorrection(correctSrc, splitIndex);
                if (correctSrc.empty()) return {};
            }
        }
        std::vector<Match> result = findTemplateLocations(correctSrc, templateName, matchDigits, splitIndex);
        matches.insert(matches.end(), result.begin(), result.end());
    }

    if (matchDigits) {
        removeMatchesWithIncorrectYCoord(matches);
    }
    sortAndRemoveOverlappingMatches(matches, matchDigits);
    return matches;
}


std::vector<TemplateMatcher::Match> TemplateMatcher::findTemplateLocations(
    const cv::UMat& src, const std::string& templateName, bool allMatchesHaveSameYCoord, int splitIndex) const {
    const Template& templateImage = templates.find(templateName)->second;
    if (src.cols < templateImage.image.cols || src.rows < templateImage.image.rows) {
        return {};
    }

    cv::UMat matchResult;
    cv::matchTemplate(src, templateImage.image, matchResult, cv::TM_SQDIFF, templateImage.mask);

    cv::UMat matchResultBinary;
    double globalMinSimilarity = getMinSimilarity(templateName, splitIndex);
    cv::threshold(matchResult, matchResultBinary, -globalMinSimilarity * templateImage.countOpaquePixels, 1, cv::THRESH_BINARY_INV);
    std::vector<cv::Point> matchPoints;
    cv::findNonZero(matchResultBinary, matchPoints);

    std::vector<Match> matches;
    cv::Mat matchResultRead = matchResult.getMat(cv::ACCESS_READ);
    for (const cv::Point& match : matchPoints) {
        int x = match.x, y = match.y;
        /* If the method of template matching is square difference (TM_SQDIFF),
         * then the lower value is a better match.
         * We want the higher similarity to be a better match, so we negate the square difference. */
        double similarity = -matchResultRead.at<float>(y, x) / templateImage.countOpaquePixels;

        if (templateName == "1") {
            // We want the right edge to be aligned with the 8x8 tile of the digit.
            x += 2;
        }

        cv::Rect matchRect(x, y, templateImage.image.cols, templateImage.image.rows);
        matches.emplace_back(matchRect, templateName, similarity);
    }

    for (Match& match : matches) {
        match.similarity *= getSimilarityMultiplier(templateName, splitIndex);
    }
    return matches;
}


double TemplateMatcher::getNewSimilarity(const cv::UMat& src, const Match& match, int splitIndex) const {
    cv::Rect location = match.location;
    if (match.templateName == "1") {
        location.x -= 2;  // Undo the change in findTemplateLocations
    }
    if (location.x + location.width > src.cols || location.y + location.height > src.rows) return -100000.;
    const Template& templateImage = templates.find(match.templateName)->second;
    cv::UMat cropped = src(location);
    cropped = convertToGray(cropped, shouldFilterYellowColor(match.templateName));
    double norm = cv::norm(cropped, templateImage.image, cv::NORM_L2SQR, templateImage.mask);
    return -norm / templateImage.countOpaquePixels * getSimilarityMultiplier(match.templateName, splitIndex);
}


void TemplateMatcher::sortAndRemoveOverlappingMatches(std::vector<Match>& matches, bool allMatchesHaveSameYCoord) {
    // Sorting the matches by similarity in descending order, and removing the overlapping ones.
    std::ranges::sort(matches, std::greater<>(), &Match::similarity);
    std::vector<Match> result;

    for (const Match& match : matches) {
        bool intersectsWithOthers = false;

        for (const Match& other : result) {
            if (allMatchesHaveSameYCoord) {
                if (std::abs(match.location.x + match.location.width - (other.location.x + other.location.width)) < 12) {
                    intersectsWithOthers = true;
                }
            } else if (!(match.location & other.location).empty()) {
                intersectsWithOthers = true;
            }

            if (intersectsWithOthers)
                break;
        }

        if (!intersectsWithOthers)
            result.push_back(match);
    }

    matches = result;
}


void TemplateMatcher::removeMatchesWithIncorrectYCoord(std::vector<Match>& matches) const {
    if (matches.empty())
        return;
    const Match& bestMatch = std::ranges::max(matches, {}, &Match::similarity);
    const cv::Rect2f& bestMatchLocation = bestMatch.location;

    std::erase_if(matches, [&](const Match& match) {
        // If the difference in the y coordinates is more than one pixel, we consider it a wrong match.
        return std::abs(match.location.y - bestMatchLocation.y) > 1;
    });
}


double TemplateMatcher::getMinSimilarity(const std::string& templateName, int splitIndex) const {
    if (templateName == TIME || templateName == SCORE) {
        return -7000;
    }
    else {
        double baselineMinSimilarity = (settings.isComposite ? -8000 : -7000);
        /* If a multiplier is present, that means that the template tends to get false matches.
         * Therefore the minimum acceptable similarity is lowered. */
        double multiplier = getSimilarityMultiplier(templateName, splitIndex);
        multiplier = std::min(multiplier, 1.5);
        return baselineMinSimilarity / multiplier;
    }
}


double TemplateMatcher::getSimilarityMultiplier(const std::string& templateName, int splitIndex) const {
    if (templateName == "1") {
        // Making one a less preferable option.
        return 2;
    } else if (templateName == "4") {
        // Four is confused with one.
        return 1.2;
    } else if (templateName == "7") {
        // Seven is confused with one and two.
        return 1.15;
    } else if (templateName == "9") {
        if (settings.isComposite && settings.game != Game::Sonic2) {
            return 1.15;
        } else {
            return 1;
        }
    } else if (splitIndex == 3 && settings.game == Game::Sonic2) {
        // Chemical Plant 2, 3 is recognized as 8 because of the white background
        if (templateName == "3") {
            return 0.9;
        } else if (templateName == "8") {
            return 1.1;
        }
    }
    // No multiplier by default.
    return 1;
}


TemplateMatcher::Template TemplateMatcher::loadTemplate(const std::string& templateName) {
    cv::UMat image = settings.loadTemplateImageFromFile(templateName);
    // We double the size of the image, as then matching is more accurate.
    cv::resize(image, image, {}, 2, 2, cv::INTER_NEAREST);

    cv::UMat grayImage = convertToGray(image, shouldFilterYellowColor(templateName));
    cv::UMat templateMask = getAlphaMask(image);
    int opaquePixels = cv::countNonZero(templateMask);

    return {grayImage, templateMask, opaquePixels};
}


cv::UMat TemplateMatcher::convertToGray(const cv::UMat& src, bool filterYellowColor) {
    if (src.channels() == 1) return src;  // already gray

    cv::UMat result;
    if (!filterYellowColor) {
        auto conversionType = (src.channels() == 3 ? cv::COLOR_BGR2GRAY : cv::COLOR_BGRA2GRAY);
        cv::cvtColor(src, result, conversionType);
    } else {
        /* Getting the yellow color intensity by adding green and red channels (BGR) and subtracting blue.
         * TIME and SCORE are yellow, so it's probably a good way to convert a frame to grayscale. */
        std::vector<cv::UMat> channels(4);
        cv::split(src, channels);
        cv::addWeighted(channels[1], 0.5, channels[2], 0.5, 0, result);
        cv::addWeighted(result, 1.0, channels[0], -0.25, 0, result);
    }

    result.convertTo(result, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.
    return result;
}


bool TemplateMatcher::shouldFilterYellowColor(const std::string& templateName) const {
    return templateName == TIME || templateName == SCORE;
}


cv::UMat TemplateMatcher::getAlphaMask(const cv::UMat& image) const {
    std::vector<cv::UMat> channels(4);
    cv::split(image, channels);

    cv::UMat mask = channels[3];  // Get the alpha channel.
    cv::threshold(mask, mask, 0, 1.0, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_8U);

    return mask;
}


cv::UMat TemplateMatcher::applyColorCorrection(const cv::UMat& src, int currentSplitIndex) const {
    cv::UMat img = src;
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
    uint8_t minBrightness = pixels[static_cast<size_t>(pixels.size() * darkPosition)];
    uint8_t maxBrightness = pixels[static_cast<size_t>(pixels.size() * brightPosition)];
    uint8_t whiteColor = pixels[static_cast<size_t>(pixels.size() * veryBrightPosition)];

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


bool TemplateMatcher::Match::operator==(const Match& other) const = default;

}  // namespace SonicVisualSplitBase
