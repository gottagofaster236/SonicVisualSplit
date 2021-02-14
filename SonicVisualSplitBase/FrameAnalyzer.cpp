#include "FrameAnalyzer.h"
#include "DigitsRecognizer.h"
#include "FrameStorage.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <algorithm>
#include <numeric>

namespace SonicVisualSplitBase {

// TODO
// Make other games work.
// Force user to open settings at least once.
// Option to set the autosplitter to practice mode, it will show "Practice" in the menu.
// (remember to fix that Begin/Stop AnalyzingFrames() works only once).
// Test EVERY option (including 16:9)
// ATEXIT: restore OBS, stop all threads.
// FUTURE:
// Auto reset option (which can be turned off for sonic 3 and stuff)
// Option to hide the component.

static std::recursive_mutex frameAnalyzationMutex;


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9) {
    if (instance == nullptr || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory || instance->isStretchedTo16By9 != isStretchedTo16By9) {
        delete instance;
        instance = new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9);
    }
    return *instance;
}


FrameAnalyzer::FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9)
    : gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9) {}


// First checks if the frame was analyzed already, otherwise calls analyzeNewFrame and caches the result.
AnalysisResult FrameAnalyzer::analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize, bool recalculateOnError) {
    if (!checkForScoreScreen && !visualize && !recalculateOnError) {
        if (FrameStorage::getResultFromCache(frameTime, result))
            return result;
    }
    AnalysisResult result = FrameAnalyzer::analyzeNewFrame(frameTime, checkForScoreScreen, visualize, recalculateOnError);
    FrameStorage::addResultToCache(result);
    return result;
}


AnalysisResult FrameAnalyzer::analyzeNewFrame(long long frameTime, bool checkForScoreScreen, bool visualize, bool recalculateOnError) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalyzationMutex);

    result = AnalysisResult();
    result.frameTime = frameTime;
    result.recognizedTime = false;

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
    switch (checkIfFrameIsSingleColor(frame)) {
    case SingleColor::BLACK:
        result.isBlackScreen = true;
        return result;
    case SingleColor::WHITE:
        result.isWhiteScreen = true;
        return result;
    }

    DigitsRecognizer& digitsRecognizer = DigitsRecognizer::getInstance(gameName, templatesDirectory);
    std::vector<std::pair<cv::Rect2f, char>> allSymbols = digitsRecognizer.findAllSymbolsLocations(frame, checkForScoreScreen);
    checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
    if (result.recognizedTime) {
        return result;
    }
    else if (recalculateOnError) {
        // we can still try to fix it - maybe video source properties have changed!
        if (!digitsRecognizer.recalculatedDigitsPlacementLastTime()) {
            DigitsRecognizer::resetDigitsPlacement();
            allSymbols = digitsRecognizer.findAllSymbolsLocations(frame, checkForScoreScreen);
            checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
        }
        if (!result.recognizedTime && visualize)
            visualizeResult(allSymbols);
    }

    if (digitsRecognizer.recalculatedDigitsPlacementLastTime() && !result.recognizedTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        DigitsRecognizer::resetDigitsPlacement();
    }
    return result;
}


void FrameAnalyzer::checkRecognizedSymbols(const std::vector<std::pair<cv::Rect2f, char>>& allSymbols, cv::UMat originalFrame, bool checkForScoreScreen, bool visualize) {
    result.recognizedTime = false;

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

    bool includesMilliseconds = (gameName == "Sonic CD" || gameName == "Knuckles' Chaotix");
    int requiredDigitsCount;
    if (includesMilliseconds)
        requiredDigitsCount = 5;
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

    std::string timeDigitsStr;
    for (auto& [position, digit] : timeDigits)
        timeDigitsStr += digit;

    if (timeDigitsStr.size() != requiredDigitsCount) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    result.recognizedTime = true;
    result.errorReason = ErrorReasonEnum::NO_ERROR;

    // Formatting the timeString and calculating timeInMilliseconds
    std::string minutes = timeDigitsStr.substr(0, 1), seconds = timeDigitsStr.substr(1, 2);
    result.timeString = minutes + "'" + seconds;
    result.timeInMilliseconds = std::stoi(minutes) * 60 * 1000 + std::stoi(seconds) * 1000;
    if (includesMilliseconds) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeString += '"' + milliseconds;
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;
    }

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


FrameAnalyzer::SingleColor FrameAnalyzer::checkIfFrameIsSingleColor(cv::UMat frame) {
    /* The algorithm is the following:
     * we take a thousand pixels from the frame, and check whether at least half of them have around the same brightness.
     * If that brightness is around 0, then we say that it's a black frame.
     * If it's high enough, we say that it's a white frame. */

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

    std::vector<int> pixelDistribution(256);
    for (int pixel : pixels) {
        pixelDistribution[pixel]++;
    }

    // We go through the pixel distribution with a sliding window of size 10, to see the most frequent brightness.
    const int SLIDING_WINDOW_SIZE = 10;

    int maximumOccurrences = INT_MIN;
    int mostPopularWindow;

    int currentOccurrences = std::accumulate(pixelDistribution.begin(), pixelDistribution.begin() + SLIDING_WINDOW_SIZE, 0);
    for (int currentWindowStart = 0; ; currentWindowStart++) {
        if (currentOccurrences > maximumOccurrences) {
            maximumOccurrences = currentOccurrences;
            mostPopularWindow = currentWindowStart;
        }

        if (currentWindowStart == pixelDistribution.size() - SLIDING_WINDOW_SIZE) {
            break;
        }
        else {
            currentOccurrences -= pixelDistribution[currentWindowStart];
            currentOccurrences += pixelDistribution[currentWindowStart + SLIDING_WINDOW_SIZE];
        }
    }

    if (maximumOccurrences < pixels.size() * 0.45)
        return SingleColor::NOT_SINGLE_COLOR;

    if (mostPopularWindow >= 70) {
        return SingleColor::WHITE;
    }
    else if (mostPopularWindow <= 35) {
        /* This is a black frame, supposedly.
         * Sonic 1's Star Light zone is dark enough to trick the algorithm, so we make another check:
         * if we precalculated the area with digits, it must dark (i.e. black frame shouldn't contain digits). */
        DigitsRecognizer* instance = DigitsRecognizer::getCurrentInstance();
        if (instance == nullptr)
            return SingleColor::NOT_SINGLE_COLOR;

        cv::Rect2f relativeDigitsRoi = instance->getRelativeDigitsRoi();
        cv::Rect digitsRoi = {(int) (relativeDigitsRoi.x * frame.cols), (int) (relativeDigitsRoi.y * frame.rows),
            (int) (relativeDigitsRoi.width * frame.cols), (int) (relativeDigitsRoi.height * frame.rows)};
        if (digitsRoi.empty())
            return SingleColor::NOT_SINGLE_COLOR;

        // Check that every pixel's brightness is low enough.
        frameRead = frameRead(digitsRoi);
        double maximumBrightness;
        cv::minMaxLoc(frameRead, nullptr, &maximumBrightness);
        if (maximumBrightness > 40)
            return SingleColor::NOT_SINGLE_COLOR;
        
        return SingleColor::BLACK;
    }
    else {
        return SingleColor::NOT_SINGLE_COLOR;
    }
}


void FrameAnalyzer::lockFrameAnalyzationMutex() {
    frameAnalyzationMutex.lock();
}


void FrameAnalyzer::unlockFrameAnalyzationMutex() {
    frameAnalyzationMutex.unlock();
}

}  // namespace SonicVisualSplitBase