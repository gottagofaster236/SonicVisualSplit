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
// Fix bug with "open layout"
// Test Sonic 3 and/or K runs
// Make OBS not restore, but minimize! Also, if OBS was maximized before, you should maximize it after restoring (not urgent).
// Game-specific ROIs to increase success rates
// Force user to open settings at least once.
// Option to set the autosplitter to practice mode, it will show "Practice" in the menu.
// (remember to fix that Begin/Stop AnalyzingFrames() works only once).
// Test EVERY option (including 16:9)
// Add a pythonanywhere server to check the current version. (OR think about adding it as a component).

/* README thoughts
 * Note: this is an early version. I've tested it on hours of footage, but it may contain bugs. So feel free to report an issue.
 * 
 * Troubleshooting: if it says "OBS Disconnected", read <link>this</link>.
 * If it has too many failed frames (when it writes a dash instead of recognized time), check your settings.
 * Make sure you've selected the correct video mode (sometimes Composite may work better than RGB).
 * If your capture card outputs a too dark image, you may have to apply a color correction filter in OBS.
 * In general, the better your video capture is, the more accurate the SVS will be.
 * Try maximizing OBS (so that the video feed has higher resolution).
 * If it's sometimes displaying wrong time, but splits correctly - that's thanks to the error checking, it's fine.
 * 
 * BUILDING:
 * VS 2019
 * vcpkg, integrate, install opencv:x64-windows
 * install LiveSplit to C:/Program Files/LiveSplit
 * Clone LiveSplit in Visual Studio
 * Clone this repository in Visual Studio
 * Copy/Paste the output dlls of LiveSplit?
 * Select the external program
 */

// ATEXIT: restore OBS, stop all threads.
// FUTURE:
// Auto reset option (which can be turned off for sonic 3 and stuff)
// Option to hide the component.

static std::recursive_mutex frameAnalyzationMutex;

static const double scaleFactorTo4By3 = (4 / 3.) / (16 / 9.);


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
                                          bool isStretchedTo16By9, bool isComposite) {
    if (instance == nullptr || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory
            || instance->isStretchedTo16By9 != isStretchedTo16By9 || instance->isComposite != isComposite) {
        delete instance;
        instance = new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9, isComposite);
    }
    return *instance;
}


FrameAnalyzer::FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory,
                             bool isStretchedTo16By9, bool isComposite)
    : gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9), isComposite(isComposite) {}


AnalysisResult FrameAnalyzer::analyzeFrame(long long frameTime, bool checkForScoreScreen, bool visualize) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalyzationMutex);

    result = AnalysisResult();
    result.frameTime = frameTime;
    result.recognizedTime = false;

    originalFrame = FrameStorage::getSavedFrame(frameTime);
    if (originalFrame.cols == 0 || originalFrame.rows == 0) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
        return result;
    }
    cv::UMat frame;
    cv::cvtColor(originalFrame, frame, cv::COLOR_BGR2GRAY);
    if (isStretchedTo16By9) {
        cv::resize(frame, frame, cv::Size(), scaleFactorTo4By3, 1, cv::INTER_AREA);
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

    DigitsRecognizer& digitsRecognizer = DigitsRecognizer::getInstance(gameName, templatesDirectory, isComposite);
    frame.convertTo(frame, CV_32F);  // Converting to CV_32F since matchTemplate does that anyways.
    recognizedSymbols = digitsRecognizer.findAllSymbolsLocations(frame, checkForScoreScreen);
    checkRecognizedSymbols(checkForScoreScreen, visualize);
    originalFrame.release();

    if (result.recognizedTime)
        return result;
    else if (visualize)
        visualizeResult();

    if (digitsRecognizer.recalculatedDigitsPlacementLastTime() && !result.recognizedTime) {
        // We recalculated everything but failed. Make sure those results aren't saved.
        DigitsRecognizer::resetDigitsPlacement();
    }
    return result;
}


void FrameAnalyzer::checkRecognizedSymbols(bool checkForScoreScreen, bool visualize) {
    result.recognizedTime = false;

    std::map<char, std::vector<cv::Rect2f>> scoreAndTimePositions;

    if (checkForScoreScreen) {
        doCheckForScoreScreen(scoreAndTimePositions);
        if (result.errorReason != ErrorReasonEnum::NO_ERROR)
            return;
    }

    std::vector<std::pair<cv::Rect2f, char>> timeDigits;
    for (auto& [position, symbol] : recognizedSymbols) {
        if (symbol != DigitsRecognizer::TIME && symbol != DigitsRecognizer::SCORE)
            timeDigits.push_back({position, symbol});
    }
    sort(timeDigits.begin(), timeDigits.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first.x < rhs.first.x;
    });

    bool includesMilliseconds = (gameName == "Sonic CD");
    int requiredDigitsCount;
    if (includesMilliseconds)
        requiredDigitsCount = 5;
    else
        requiredDigitsCount = 3;

    for (int i = 0; i + 1 < timeDigits.size(); i++) {
        double bestScale = DigitsRecognizer::getCurrentInstance()->getBestScale();
        double interval = ((timeDigits[i + 1].first.x + timeDigits[i + 1].first.width) -
            (timeDigits[i].first.x + timeDigits[i].first.width)) * bestScale;
        
        if (i == 0 || i == 2) {
            /* Checking that separators (:, ' and ") aren't detected as digits.
             * For that we check that the digits adjacent to the separator have increased interval. */
            if (interval < 28) {
                timeDigits.erase(timeDigits.begin() + i + 1);
                i--;
            }
        }
        else {
            // Checking that the interval is not too high.
            if (interval > 20) {
                result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
                return;
            }
        }
    }

    if (timeDigits.size() != requiredDigitsCount) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }

    std::string timeDigitsStr;
    for (auto& [position, digit] : timeDigits)
        timeDigitsStr += digit;

    // Formatting the timeString and calculating timeInMilliseconds
    std::string minutes = timeDigitsStr.substr(0, 1), seconds = timeDigitsStr.substr(1, 2);
    if (std::stoi(seconds) >= 60) {
        // The number of seconds is always 59 or lower.
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    result.timeString = minutes + "'" + seconds;
    result.timeInMilliseconds = std::stoi(minutes) * 60 * 1000 + std::stoi(seconds) * 1000;
    if (includesMilliseconds) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeString += '"' + milliseconds;
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;

        if (result.timeInMilliseconds <= 30) {
            /* Sonic CD starts the timer from 0'00"03 (or 0'00"01) when you die.
             * This check may be deleted as Sonic CD mods will clarify the rules. */ 
            result.timeInMilliseconds = 0;
        }
    }

    result.recognizedTime = true;
    result.errorReason = ErrorReasonEnum::NO_ERROR;

    if (visualize) {
        recognizedSymbols = timeDigits;
        char additionalSymbols[] = {DigitsRecognizer::SCORE, DigitsRecognizer::TIME};
        for (char c : additionalSymbols) {
            for (const cv::Rect2f& position : scoreAndTimePositions[c]) {
                recognizedSymbols.push_back({position, c});
            }
        }
        visualizeResult();
    }
}


void FrameAnalyzer::doCheckForScoreScreen(std::map<char, std::vector<cv::Rect2f>>& scoreAndTimePositions) {
    for (auto& [position, symbol] : recognizedSymbols) {
        if (symbol == DigitsRecognizer::SCORE || symbol == DigitsRecognizer::TIME) {
            scoreAndTimePositions[symbol].push_back(position);
        }
    }

    if (scoreAndTimePositions[DigitsRecognizer::SCORE].empty() || scoreAndTimePositions[DigitsRecognizer::TIME].empty()) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    std::vector<cv::Rect2f>& timeRects = scoreAndTimePositions[DigitsRecognizer::TIME];
    cv::Rect2f timeRect = *std::min_element(timeRects.begin(), timeRects.end(), [](const auto& rect1, const auto& rect2) {
        return rect1.y < rect2.y;
    });

    if (timeRects.size() >= 2) {
        // make sure that the other recognized TIME rectangles are valid
        for (int i = timeRects.size() - 1; i >= 0; i--) {
            if (timeRects[i] == timeRect)
                continue;
            if (timeRects[i].y - timeRect.y < timeRect.width || timeRects[i].y > 3 * originalFrame.rows / 4) {
                timeRects.erase(timeRects.begin() + i);
            }
        }
    }

    // There's always a "TIME" label on top of the screen
    // But there's a second one during the score countdown screen ("TIME BONUS")
    result.isScoreScreen = (timeRects.size() >= 2);
}


void FrameAnalyzer::visualizeResult() {
    for (auto& [position, symbol] : recognizedSymbols) {
        if (isStretchedTo16By9) {
            position.x /= scaleFactorTo4By3;
            position.width /= scaleFactorTo4By3;
        }
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
    
    std::vector<int> pixelDistribution(256);  // How many pixels have the corresponding brightness value.
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

    if (mostPopularWindow >= 200) {
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