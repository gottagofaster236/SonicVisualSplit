#include "FrameAnalyzer.h"
#include "DigitsRecognizer.h"
#include "FrameStorage.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>

namespace SonicVisualSplitBase {

// TODO
// Save the frame size sometime (to understand the minimum size mb). Not urgent? IDK
// if the time increases by more than the time passed (+1?), ignore that!!!!!! (maybe also check the prev-prev-time, idk)
// ATEXIT: restore OBS, stop all threads.
// if the time got down without the "could not recognize", ignore that
// double-check score (both the presense and the time). Split if they both match (presense and time)
// do something with split undos. On each split, calculate the sum of times!
// for the future: think about the end of a run; maybe detect "sega"? scaled down mb.
// Test EVERY option (including 16:9)
// set bestScale to -1 when the source is changed
// sometimes it fails???
// auto reset option (which can be turned off for sonic 3 and stuff)

// ===================
// options screen:
// Live preview
// Video settings:
// composite / rf or rgb
// 4:3 or 16:9 stretched
// ===================


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9) {
    if (instance == nullptr || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory || instance->isStretchedTo16By9 != isStretchedTo16By9) {
        delete instance;
        instance = new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9);
    }
    return *instance;
}


FrameAnalyzer::FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9)
    : gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9) {}



AnalysisResult FrameAnalyzer::analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize, bool recalculateOnError) {
    result = AnalysisResult();
    result.foundAnyDigits = false;

    cv::UMat originalFrame = FrameStorage::getSavedFrame(frameTime);
    if (originalFrame.cols == 0 || originalFrame.rows == 0) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
        return result;
    }
    cv::UMat frame;
    cv::cvtColor(originalFrame, frame, cv::COLOR_BGR2GRAY);
    if (isStretchedTo16By9) {
        const double scaleFactor = (4 / 3.) / (16 / 9.);
        cv::resize(frame, frame, cv::Size(), scaleFactor, 1, cv::INTER_AREA);
    }
    if (visualize) {
        originalFrame.copyTo(result.visualizedFrame);
    }
    if (checkIfFrameIsBlack(frame)) {
        result.isBlackScreen = true;
        return result;
    }

    DigitsRecognizer& digitsRecognizer = DigitsRecognizer::getInstance(gameName, templatesDirectory);
    std::vector<std::pair<cv::Rect2f, char>> allSymbols = digitsRecognizer.findAllSymbolsLocations(frame, checkForScoreScreen);
    checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
    if (result.foundAnyDigits) {
        return result;
    }
    else if (recalculateOnError) {
        // we can still try to fix it - maybe video source properties have changed!
        if (!digitsRecognizer.recalculatedDigitsPlacementLastTime()) {
            digitsRecognizer.resetDigitsPlacement();
            allSymbols = digitsRecognizer.findAllSymbolsLocations(frame, checkForScoreScreen);
            checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
        }
        if (!result.foundAnyDigits && visualize)
            visualizeResult(allSymbols);
    }
    return result;
}


void FrameAnalyzer::checkRecognizedSymbols(const std::vector<std::pair<cv::Rect2f, char>>& allSymbols, cv::UMat originalFrame, bool checkForScoreScreen, bool visualize) {
    result.foundAnyDigits = false;

    std::map<char, std::vector<cv::Rect2f>> positionsOfSymbol;

    if (checkForScoreScreen) {
        for (auto& [position, symbol] : allSymbols) {
            if (symbol == DigitsRecognizer::SCORE || symbol == DigitsRecognizer::TIME) {
                positionsOfSymbol[symbol].push_back(position);
            }
        }

        // score is divisible by 10, so there must be a zero on the screen
        if (positionsOfSymbol[DigitsRecognizer::SCORE].empty() || positionsOfSymbol[DigitsRecognizer::TIME].empty()) {
            result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
            return;
        }
        std::vector<cv::Rect2f>& timeRects = positionsOfSymbol[DigitsRecognizer::TIME];
        cv::Rect2f timeRect = *std::min_element(timeRects.begin(), timeRects.end(), [](const auto& rect1, const auto& rect2) {
            return rect1.y < rect2.y;
        });

        if (timeRects.size() >= 2) {
            // make sure that the other recognized TIME rectangles are valid
            for (int i = timeRects.size() - 1; i >= 0; i--) {
                if (timeRects[i] == timeRect)
                    continue;
                if (timeRects[i].y - timeRect.y < timeRect.width || timeRects[i].y > 2 * originalFrame.rows / 3) {
                    timeRects.erase(timeRects.begin() + i);
                }
            }
        }

        // There's always a "TIME" label on top of the screen
        // But there's a second one during the score countdown screen ("TIME BONUS")
        result.isScoreScreen = (timeRects.size() >= 2);
    }

    std::vector<std::pair<cv::Rect2f, char>> timeDigits;
    for (auto& [position, symbol] : allSymbols) {
        if (symbol != DigitsRecognizer::TIME && symbol != DigitsRecognizer::SCORE)
            timeDigits.push_back({position, symbol});
    }
    sort(timeDigits.begin(), timeDigits.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first.x < rhs.first.x;
    });

    int requiredDigitsCount;
    if (gameName == "Sonic CD" || gameName == "Knuckles' Chaotix")
        requiredDigitsCount = 5;  // includes milliseconds
    else
        requiredDigitsCount = 3;

    // Try remove excess digits. Check that the digits aren't too far from each other.
    for (int i = 0; i + 1 < timeDigits.size(); i++) {
        float distanceX = timeDigits[i + 1].first.x - timeDigits[i].first.x;
        if (distanceX >= 2 * timeDigits[i].first.height) {
            timeDigits.erase(timeDigits.begin() + i + 1, timeDigits.end());
            break;
        }
    }

    if (gameName == "Sonic 1") {
        // Sonic 1 has a really small "1" sprite, it sometimes recognizes that as a result of error
        while (timeDigits.size() > requiredDigitsCount && timeDigits.back().second == '1')
            timeDigits.pop_back();
    }

    for (auto& [position, digit] : timeDigits)
        result.timeDigits += digit;

    if (result.timeDigits.size() != requiredDigitsCount) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }

    result.foundAnyDigits = true;
    result.errorReason = ErrorReasonEnum::NO_ERROR;

    if (visualize) {
        auto recognizedSymbols = timeDigits;
        char additionalSymbols[] = {DigitsRecognizer::SCORE, DigitsRecognizer::TIME};
        for (char c : additionalSymbols) {
            for (const cv::Rect2f& position : positionsOfSymbol[c]) {
                recognizedSymbols.push_back({position, c});
            }
        }
        visualizeResult(recognizedSymbols);
    }
}


void FrameAnalyzer::visualizeResult(const std::vector<std::pair<cv::Rect2f, char>>& symbols) {
    for (auto& [position, symbol] : symbols) {
        cv::rectangle(result.visualizedFrame, position, cv::Scalar(0, 0, 255), 1);
    }
}


bool FrameAnalyzer::checkIfFrameIsBlack(cv::UMat frame) {
    // calculating the median value out of 1000 pixels, and checking if it's dark enough
    cv::Mat frameRead = frame.getMat(cv::ACCESS_READ);
    const int pixelsPerDimension = 30;
    int stepX = std::max(frame.cols / pixelsPerDimension, 1);
    int stepY = std::max(frame.rows / pixelsPerDimension, 1);
    std::vector<uint8_t> pixels;
    for (int y = 0; y < frame.rows; y += stepY) {
        for (int x = 0; x < frame.cols; x += stepX) {
            pixels.push_back(frameRead.at<uint8_t>(y, x));
        }
    }
    int medianPosition = pixels.size() / 2;
    std::nth_element(pixels.begin(), pixels.begin() + medianPosition, pixels.end());
    int median = pixels[medianPosition];
    return median <= 10;
}

}  // namespace SonicVisualSplitBase