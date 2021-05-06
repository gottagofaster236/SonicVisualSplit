﻿#include "FrameAnalyzer.h"
#include "DigitsRecognizer.h"
#include "FrameStorage.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <algorithm>
#include <numeric>


namespace SonicVisualSplitBase {

static std::recursive_mutex frameAnalyzationMutex;

static const double scaleFactorTo4By3 = (4 / 3.) / (16 / 9.);


FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, 
        bool isStretchedTo16By9, bool isComposite) {
    std::lock_guard<std::recursive_mutex> guard(frameAnalyzationMutex);

    if (!instance || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory
            || instance->isStretchedTo16By9 != isStretchedTo16By9 || instance->isComposite != isComposite) {
        instance = std::unique_ptr<FrameAnalyzer>(
            new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9, isComposite));
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

    // Make sure the frame isn't too large (that'll slow down the calculations).
    if (originalFrame.rows > DigitsRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) {
        double scaleFactor = ((double) DigitsRecognizer::MAX_ACCEPTABLE_FRAME_HEIGHT) / originalFrame.rows;
        cv::resize(originalFrame, originalFrame, cv::Size(), scaleFactor, scaleFactor, cv::INTER_AREA);
    }

    cv::UMat frame = originalFrame;
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
    recognizedSymbols = digitsRecognizer.findLabelsAndDigits(frame, checkForScoreScreen);
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

    std::vector<DigitsRecognizer::Match> timeDigits;
    for (const auto& match : recognizedSymbols) {
        if (match.symbol != DigitsRecognizer::TIME && match.symbol != DigitsRecognizer::SCORE)
            timeDigits.push_back(match);
    }
    std::ranges::sort(timeDigits, {}, [](const auto& match) {
        return match.location.x;
    });

    bool includesMilliseconds = (gameName == "Sonic CD");
    int requiredDigitsCount;
    if (includesMilliseconds)
        requiredDigitsCount = 5;
    else
        requiredDigitsCount = 3;

    for (int i = 0; i + 1 < timeDigits.size(); i++) {
        double bestScale = DigitsRecognizer::getCurrentInstance()->getBestScale();
        const cv::Rect2f& prevLocation = timeDigits[i].location, nextLocation = timeDigits[i + 1].location;
        double interval = ((nextLocation.x + nextLocation.width) -
            (prevLocation.x + prevLocation.width)) * bestScale;
        
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

    if (isComposite) {
        while (timeDigits.size() > requiredDigitsCount && timeDigits.back().symbol == '1')
            timeDigits.pop_back();
    }

    if (timeDigits.size() != requiredDigitsCount) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }

    std::string timeDigitsStr;
    for (const auto& match : timeDigits)
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
    if (includesMilliseconds) {
        std::string milliseconds = timeDigitsStr.substr(3, 2);
        result.timeString += '"' + milliseconds;
        result.timeInMilliseconds += std::stoi(milliseconds) * 10;
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
    for (const auto& match : recognizedSymbols) {
        if (match.symbol == DigitsRecognizer::SCORE || match.symbol == DigitsRecognizer::TIME) {
            scoreAndTimePositions[match.symbol].push_back(match.location);
        }
    }

    if (scoreAndTimePositions[DigitsRecognizer::SCORE].empty() || scoreAndTimePositions[DigitsRecognizer::TIME].empty()) {
        result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
        return;
    }
    std::vector<cv::Rect2f>& timeRects = scoreAndTimePositions[DigitsRecognizer::TIME];
    cv::Rect2f timeRect = *std::ranges::min_element(timeRects, [](const auto& rect1, const auto& rect2) {
        return rect1.y < rect2.y;
    });

    if (timeRects.size() >= 2) {
        // Make sure that the other recognized TIME rectangles are valid.
        for (int i = (int) timeRects.size() - 1; i >= 0; i--) {
            if (timeRects[i] == timeRect)
                continue;
            if (timeRects[i].y - timeRect.y < timeRect.width || timeRects[i].y > 3 * originalFrame.rows / 4) {
                timeRects.erase(timeRects.begin() + i);
            }
        }
    }

    /* There's always a "TIME" label on top of the screen.
     * But there's a second one during the score countdown screen ("TIME BONUS"). */
    result.isScoreScreen = (timeRects.size() >= 2);
}


void FrameAnalyzer::visualizeResult() {
    int lineThickness = 1 + result.visualizedFrame.rows / 500;
    for (auto match : recognizedSymbols) {
        if (isStretchedTo16By9) {
            match.location.x /= scaleFactorTo4By3;
            match.location.width /= scaleFactorTo4By3;
        }
        cv::rectangle(result.visualizedFrame, match.location, cv::Scalar(0, 0, 255), lineThickness);
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
            cv::Vec3b pixel = frameRead.at<cv::Vec3b>(y, x);
            uint8_t brightness = ((int) pixel[0] + (int) pixel[1] + (int) pixel[2]) / 3;
            pixels.push_back(brightness);
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

    if (maximumOccurrences < pixels.size() * 0.35)
        return SingleColor::NOT_SINGLE_COLOR;

    if (mostPopularWindow >= 200) {
        return SingleColor::WHITE;
    }
    else if (mostPopularWindow <= 35) {
        /* This is a black frame, supposedly.
         * Sonic 1's Star Light zone is dark enough to trick the algorithm, so we make another check:
         * if we precalculated the area with digits, it must dark (i.e. black frame shouldn't contain digits). */
        const std::unique_ptr<DigitsRecognizer>& instance = DigitsRecognizer::getCurrentInstance();
        if (!instance)
            return SingleColor::NOT_SINGLE_COLOR;

        cv::Rect2f relativeDigitsRect = instance->getRelativeDigitsRect();
        cv::Rect digitsRect = {(int) (relativeDigitsRect.x * frame.cols), (int) (relativeDigitsRect.y * frame.rows),
            (int) (relativeDigitsRect.width * frame.cols), (int) (relativeDigitsRect.height * frame.rows)};
        if (digitsRect.empty())
            return SingleColor::NOT_SINGLE_COLOR;

        // Check that every pixel's brightness is low enough.
        frameRead = frameRead(digitsRect);
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


void FrameAnalyzer::reportCurrentSplitIndex(int currentSplitIndex) {
    FrameAnalyzer::currentSplitIndex = currentSplitIndex;
}


int FrameAnalyzer::getCurrentSplitIndex() {
    return currentSplitIndex;
}


void FrameAnalyzer::lockFrameAnalyzationMutex() {
    frameAnalyzationMutex.lock();
}


void FrameAnalyzer::unlockFrameAnalyzationMutex() {
    frameAnalyzationMutex.unlock();
}

}  // namespace SonicVisualSplitBase