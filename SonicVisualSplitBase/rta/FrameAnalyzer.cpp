#include "FrameAnalyzer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <array>
#include <chrono>
#include <thread>

namespace SonicVisualSplitBase {
namespace RTA {

// Avoid several consecutive splits at once
constexpr long long WAIT_AFTER_SPLIT_MS = 3000;
constexpr long long TIME_BONUS_CHECK_PERIOD_MS = 300;
constexpr double MIN_BRIGHTNESS_DECREASE_PART = 0.25;
constexpr double MIN_BRIGHTNESS_DECREASE_PART_WITHOUT_TIME_BONUS = 0.6;
constexpr double MIN_BRIGHTNESS_DECREASE_PART_SEGA = 0.6;
constexpr double MAX_BRIGHTNESS_INCREASE_PART = 0.15;

using VideoCaptureManager::CapturedFrame;

FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings, Callback& callback) :
    settings(settings), callback(callback), onFrameCapturedListener(*this), 
    templateMatcher(settings, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", TemplateMatcher::TIME}) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}


FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
    analysisThreadPool.clear_tasks();
    analysisThreadPool.wait_for_tasks();
}


bool FrameAnalyzer::isSupportedGame() const {
    return settings.game == Game::Sonic1 || settings.game == Game::Sonic2;
}

// includeGameEnding - configurable because it's handled by detectRunFinish normally
bool FrameAnalyzer::splitWithoutTimeBonus(int splitIndex, bool includeGameEnding) const {
    if (settings.game == Game::Sonic1)
    {
        // Sonic 1's Scrap Brain 3 doesn't have the proper transition, neither does Final Zone.
        return splitIndex == 17 || (includeGameEnding && splitIndex == 18);
    } else if (settings.game == Game::Sonic2)
    {
        // Same for Sonic 2's Sky Chase and Wing Fortress. Death Egg fades to white so we ignore it here.
        return splitIndex == 17 || splitIndex == 18;
    }
    return false;
}


void FrameAnalyzer::reportSplitIndex(int splitIndex) {
    std::lock_guard guard(analysisMutex);
    if (this->splitIndex == splitIndex) return;
    this->splitIndex = splitIndex;
    timeBonusState = TimeBonusState::INITIAL;
    timeBonusDetected = false;
}


static bool detectFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame, const cv::Rect& gameRect, bool splitWithoutTimeBonus);

void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (!isSupportedGame()) return;
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }

    cv::Rect gameRect;
    bool timerStarted;
    {
        std::lock_guard guard(analysisMutex);
        timerStarted = splitIndex != -1;
        gameRect = this->gameRect;
    }

    // Re-detect gameRect if there is none, or if timer is reset. Otherwise keep using the same gameRect to avoid slowdown.
    if (gameRect.empty() || !timerStarted) {
        cv::Rect gameRectOnFade = detectGameRectOnFade(currentFrame, previousFrame);
        if (gameRectOnFade.empty()) return;
        if (detectGameRectOnSegaScreen(previousFrame, gameRectOnFade)) {
            if (timerStarted && settings.autoResetEnabled) callback.reset();
            return;
        }
    }

    cv::UMat currentFrameGameRect;
    int splitIndex;
    {
        std::lock_guard guard(analysisMutex);
        if (this->gameRect.empty() || gameRectFrameSize != previousFrame.frame.size()) return;
        gameRect = this->gameRect;
        currentFrameGameRect = currentFrame.frame(gameRect);
        splitIndex = this->splitIndex;
    }

    bool detectedFade = detectFade(currentFrame, previousFrame, gameRect, splitWithoutTimeBonus(splitIndex, false));
    bool detectedRunFinish = !detectedFade && detectRunFinish(currentFrameGameRect, currentFrame.timestamp);
    if (detectedFade || detectedRunFinish) {
        {
            if (!timerStarted) {
                if (!isTitleScreen(previousFrame.frame(gameRect))) return;
            } else if (detectGameRectOnSegaScreen(previousFrame, gameRect)) {
                if (settings.autoResetEnabled) callback.reset();
                return;
            }
            std::lock_guard guard(analysisMutex);
            if (currentFrame.timestamp - lastSplitTime < WAIT_AFTER_SPLIT_MS) return;
            if (timerStarted && !detectedRunFinish && !timeBonusDetected && !splitWithoutTimeBonus(splitIndex, false)) return;
            lastSplitTime = currentFrame.timestamp;
        }

        if (!timerStarted) {
            callback.startTimer();
        } else {
            callback.split();
        }
    }

    processTimeBonus(currentFrameGameRect, currentFrame.timestamp);
    processLevelStart(currentFrameGameRect, currentFrame.timestamp);
}

static cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold = 3.0);
static double maskNonZeroPart(const cv::UMat& mask);

/**
 * currentFrame and previousFrame have to be the same size.
 * Detects the game rectangle on the screen if the game is currently fading to black
 * (on game start after title screen, after time bonus countdown, after death).
 * This is usually also the point to split in RTA-TB.
 * Returns an empty rect if a fade to black isn't going on or on detection failure.
 */
cv::Rect FrameAnalyzer::detectGameRectOnFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) const {
    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousFrame.frame, currentFrame.frame);
    // The game can take a part of the screen. Also fading has a limited color pallete, so only a part of the image fades.
    if (maskNonZeroPart(decreasedBrightnessMask) < 0.15) return {};

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(decreasedBrightnessMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return {};
    const std::vector<cv::Point>& largestContour = std::ranges::max(contours, {}, [](const std::vector<cv::Point>& contour) {
        return cv::contourArea(contour);
    });
    cv::Rect gameRect = cv::boundingRect(largestContour);

    if (!verifyGameRectAspectRatio(gameRect)) return {};
    if (gameRect.height < currentFrame.frame.rows / 2) return {};

    if (maskNonZeroPart(decreasedBrightnessMask(gameRect)) < MIN_BRIGHTNESS_DECREASE_PART_SEGA) return {};
    cv::UMat gameRectBrightnessIncrease =
        calculateIncreasedBrightnessMask(currentFrame.frame(gameRect), previousFrame.frame(gameRect));
    if (maskNonZeroPart(gameRectBrightnessIncrease) > MAX_BRIGHTNESS_INCREASE_PART) return {};
    return gameRect;
}


// mask should be 8-bit single channel, with values being either 0 or 255.
double maskNonZeroPart(const cv::UMat& mask) {
    return cv::mean(mask)[0] / 255.0;
}


// src1 and src2 must have three 8-bit channels.
// Returns a single-channel 8-bit mask of pixels that increased from src2 to src1 by more than the `threshold`.
cv::UMat calculateIncreasedBrightnessMask(cv::UMat src1, cv::UMat src2, double threshold) {
    cv::UMat difference;
    cv::subtract(src1, src2, difference, cv::noArray(), CV_32S);
    std::vector<cv::UMat> channels;
    channels.reserve(3);
    cv::split(difference, channels);
    assert(channels.size() == 3);
    cv::add(channels[0], channels[1], channels[0]);
    cv::add(channels[0], channels[2], channels[0]);
    cv::UMat thresholdResult = channels[0];
    thresholdResult.convertTo(thresholdResult, CV_8U);
    cv::threshold(thresholdResult, thresholdResult, threshold / 3, 255.0, cv::THRESH_BINARY);
    return thresholdResult;
}


// This is more precise than the rectangle on fade
bool FrameAnalyzer::detectGameRectOnSegaScreen(const VideoCaptureManager::CapturedFrame& frame, const cv::Rect& oldGameRect) {
    cv::UMat oldGameScreen = frame.frame(oldGameRect);
    cv::UMat whiteColorMask;
    cv::inRange(oldGameScreen, cv::Scalar(180, 180, 180), cv::Scalar(255, 255, 255), whiteColorMask);
    if (maskNonZeroPart(whiteColorMask) < 0.6) return false;
    cv::UMat blueColorMask;
    switch (settings.game) {
    case Game::Sonic1:
        cv::inRange(oldGameScreen, cv::Scalar(180, 0, 0), cv::Scalar(255, 30, 30), blueColorMask);
        break;
    default:
        cv::inRange(oldGameScreen, cv::Scalar(170, 60, 0), cv::Scalar(255, 150, 30), blueColorMask);
        break;
    }
    if (maskNonZeroPart(blueColorMask) < 0.04) return false;

    cv::Rect withoutBorders = oldGameRect;
    int cropWidth = withoutBorders.height / 5;
    withoutBorders.x += cropWidth;
    withoutBorders.y += cropWidth;
    withoutBorders.width -= 2 * cropWidth;
    withoutBorders.height -= 2 * cropWidth;
    if (withoutBorders.empty()) return false;
    cv::inRange(frame.frame(withoutBorders), cv::Scalar(50, 0, 0), cv::Scalar(255, 255, 100), blueColorMask);

    cv::Rect segaLogo = cv::boundingRect(blueColorMask);
    segaLogo.x += withoutBorders.x;
    segaLogo.y += withoutBorders.y;
    if (segaLogo.empty()) return false;
    cv::Rect gameRect{
        segaLogo.x - static_cast<int>(std::round(segaLogo.width / 374. * 130.)),
        segaLogo.y - static_cast<int>(std::round(segaLogo.height / 116. * (182. - 16.))),
        static_cast<int>(std::round(segaLogo.width / 374. * 640.)),
        static_cast<int>(std::round(segaLogo.height / 116. * (480. - 32.))),
    };
    gameRect &= cv::Rect{0, 0, frame.frame.cols, frame.frame.rows};
    if (!verifyGameRectAspectRatio(gameRect)) return false;

    std::lock_guard guard(analysisMutex);
    // Prefer the first game rectangle, as it operates with higher brightness and thus more accurate
    if (frame.timestamp - gameRectTimestamp >= WAIT_AFTER_SPLIT_MS) {
        this->gameRect = gameRect;
        gameRectFrameSize = frame.frame.size();
        gameRectTimestamp = frame.timestamp;
        callback.onGameRectUpdated(gameRect);
        return true;
    }
    return false;
}

bool FrameAnalyzer::verifyGameRectAspectRatio(const cv::Rect& gameRect) const {
    if (gameRect.empty()) return {};
    double aspectRatio = ((double) gameRect.width) / gameRect.height;
    double expectedAspectRatio = settings.isStretchedTo16By9 ? (16. / 9) : (4. / 3);
    return std::abs(aspectRatio - expectedAspectRatio) < 0.15;
}


bool detectFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame, const cv::Rect& gameRect, bool splitWithoutTimeBonus) {
    cv::UMat currentScreen = currentFrame.frame(gameRect);
    cv::UMat previousScreen = previousFrame.frame(gameRect);
    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousScreen, currentScreen);
    double decreasedBrightness = maskNonZeroPart(decreasedBrightnessMask);
    if (decreasedBrightness <
        (splitWithoutTimeBonus ? MIN_BRIGHTNESS_DECREASE_PART_WITHOUT_TIME_BONUS : MIN_BRIGHTNESS_DECREASE_PART)) return false;
    cv::UMat increasedBrightnessMask = calculateIncreasedBrightnessMask(currentScreen, previousScreen);
    double increasedBrightness = maskNonZeroPart(increasedBrightnessMask);
    return increasedBrightness < MAX_BRIGHTNESS_INCREASE_PART && increasedBrightness < decreasedBrightness / 3.0;
}

bool FrameAnalyzer::isTitleScreen(const cv::UMat& gameScreen) {
    // Both Sonic 1 and 2 have red backgrounds behind letters on the title screen.
    cv::UMat redColorMask;
    cv::inRange(gameScreen, cv::Scalar(0, 0, 80), cv::Scalar(30, 30, 240), redColorMask);
    if (maskNonZeroPart(redColorMask) < 0.015) return false;
    if (settings.game == Game::Sonic2) {
        // Prevent starting on title screen fading from white to blue if you don't skip it with start
        cv::UMat blueColorMask;
        cv::inRange(gameScreen, cv::Scalar(170, 0, 0), cv::Scalar(255, 70, 30), blueColorMask);  // 0.48
        if (maskNonZeroPart(blueColorMask) < 0.25) return false;
    }
    return true;
}


void FrameAnalyzer::processTimeBonus(const cv::UMat& gameRect, long long timestamp) {
    TimeBonusState timeBonusState;
    {
        std::lock_guard guard(analysisMutex);
        timeBonusState = this->timeBonusState;
    }

    switch (timeBonusState) {
    case TimeBonusState::INITIAL:
        {
            std::lock_guard guard(analysisMutex);
            if (timestamp - lastTimeBonusCheckTime < TIME_BONUS_CHECK_PERIOD_MS) return;
            lastTimeBonusCheckTime = timestamp;
        }
        if (hasTimeBonus(gameRect)) {
            std::lock_guard guard(analysisMutex);
            if (this->timeBonusState == TimeBonusState::INITIAL) {
                this->timeBonusState = TimeBonusState::CONFIRM_TIME_BONUS_LABEL;
            }
        }
        break;
    case TimeBonusState::CONFIRM_TIME_BONUS_LABEL:
    case TimeBonusState::CONFIRM_TIME_BONUS_POINTS:
    {
        std::string lastTimeBonusString;
        {
            std::lock_guard guard(analysisMutex);
            if (timestamp - lastTimeBonusCheckTime < TIME_BONUS_CHECK_PERIOD_MS) return;
            lastTimeBonusCheckTime = timestamp;
            lastTimeBonusString = timeBonusString;
        }
        bool foundTimeBonusPoints = getTimeBonusPoints(gameRect);
        int timeBonusPoints = 0;
        {
            std::lock_guard guard(analysisMutex);
            bool timeBonusStringChanged = timeBonusString != lastTimeBonusString;
            if (!foundTimeBonusPoints || (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_POINTS && timeBonusStringChanged)) {
                this->timeBonusState = TimeBonusState::INITIAL;
            } else if (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_LABEL) {
                this->timeBonusState = TimeBonusState::CONFIRM_TIME_BONUS_POINTS;
            } else if (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_POINTS) {
                this->timeBonusState = TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START;
                timeBonusPoints = this->timeBonusPoints;
                this->timeBonusDetected = true;
            }
        }
        if (timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_POINTS) {
            callback.onAnalysisResult(AnalysisResult(timestamp, timeBonusPoints));
        }
        break;
    }
    case TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START:
    {
        TemplateMatcher::Match digitMatchToObserve;
        bool fallbackNoCountdown;
        {
            std::lock_guard guard(analysisMutex);
            fallbackNoCountdown = timestamp - lastTimeBonusCheckTime > 3000;
            digitMatchToObserve = this->digitMatchToObserve;
        }
        if (fallbackNoCountdown ||
            templateMatcher.getNewSimilarity(cropToDigitsRect(gameRect), digitMatchToObserve) < 1.5 * digitMatchToObserve.similarity - 1000) {
            {
                std::lock_guard guard(analysisMutex);
                if (this->timeBonusState != TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START) break;
                this->timeBonusState = TimeBonusState::PAUSED_FOR_TIME_BONUS;
                timeBonusPoints = this->timeBonusPoints;
            }
            pauseForTimeBonus(timeBonusPoints);
            {
                std::lock_guard guard(analysisMutex);
                if (this->timeBonusState == TimeBonusState::PAUSED_FOR_TIME_BONUS) {
                    if (this->timeBonusPoints != 0) {
                        this->timeBonusState = TimeBonusState::AFTER_TIME_BONUS;
                    } else {
                        // 0 points can be detected erroneously unfortunately
                        this->timeBonusState = TimeBonusState::INITIAL;
                    }
                }
            }
        }
        break;
    }
    default: {}
    }
}


static cv::UMat cropGameRectAndResize(const cv::UMat& gameRect, int x, int y, int width, int height, bool filterYellowColor);

static cv::UMat cropGameRect(const cv::UMat& gameRect, int x, int y, int width, int height);


bool FrameAnalyzer::getTimeBonusPoints(const cv::UMat& gameRect) {
    // TODO: visualization, maybe save matches to a field?
    if (!hasTimeBonus(gameRect)) return false;

    cv::UMat digitsRect = cropToDigitsRect(gameRect);
    std::vector<TemplateMatcher::Match> matches = templateMatcher.findTemplateLocations(digitsRect, TemplateMatcher::DIGIT_TEMPLATES, true);
    if (matches.empty()) return false;

    std::ranges::sort(matches, {}, [](const TemplateMatcher::Match& match) { return match.location.x; });
    std::string timeBonusString;
    for (const TemplateMatcher::Match& match : matches) {
        timeBonusString += match.templateName;
    }

    const std::vector<std::string> allowedTimeBonuses = {"100000", "50000", "10000", "5000", "4000", "3000", "2000", "1000", "500", "0"};
    for (const std::string& possibleTimeBonus : allowedTimeBonuses) {
        if (timeBonusString.ends_with(possibleTimeBonus)) {
            TemplateMatcher::Match digitMatchToObserve = matches[timeBonusString.size() - possibleTimeBonus.size()];
            // Because color correction depends on the whole rect, it's different from getNewSimilarity.
            digitMatchToObserve.similarity = templateMatcher.getNewSimilarity(digitsRect, digitMatchToObserve);
            std::lock_guard guard(analysisMutex);
            this->timeBonusPoints = std::stoi(possibleTimeBonus);
            this->timeBonusString = timeBonusString;
            this->digitMatchToObserve = digitMatchToObserve;
            return true;
        }
    }
    return false;
}


cv::UMat FrameAnalyzer::cropToDigitsRect(const cv::UMat& gameRect) const {
    if (settings.game == Game::Sonic1) {
        return cropGameRectAndResize(gameRect, 382, 230, 102, 30, false);
    } else {
        return cropGameRectAndResize(gameRect, 412, 222, 104, 30, false);
    }
}


bool FrameAnalyzer::hasTimeBonus(const cv::UMat& gameRect) const {
    cv::UMat timeMatchRect;
    if (settings.game == Game::Sonic1) {
        timeMatchRect = cropGameRectAndResize(gameRect, 158, 230, 70, 30, true);
    } else {
        timeMatchRect = cropGameRectAndResize(gameRect, 134, 222, 68, 30, true);
    }
    return !templateMatcher.findTemplateLocations(timeMatchRect, {TemplateMatcher::TIME}, false).empty();
}


void FrameAnalyzer::pauseForTimeBonus(int timeBonusPoints) {
    if (timeBonusPoints == 0) return;
    int framesToPause = timeBonusPoints / 100;
    if (settings.game == Game::Sonic2 && timeBonusPoints >= 10000) {
        // Add frames for continue
        framesToPause += 120;
    }
    // Sega Genesis has a framerate different from the standard NTSC 59.94 one
    std::chrono::microseconds pauseDuration{static_cast<long long>(1000 * 1000 * framesToPause / 59.9228)};
    auto start = std::chrono::steady_clock::now();
    callback.pauseTimer();
    std::this_thread::sleep_until(start + pauseDuration);
    callback.unpauseTimer();
}


// See cropGameRect
cv::UMat cropGameRectAndResize(const cv::UMat& gameRect, int x, int y, int width, int height, bool filterYellowColor) {
    cv::UMat cropped = cropGameRect(gameRect, x, y, width, height);
    cropped = TemplateMatcher::convertToGray(cropped, filterYellowColor);  // Convert to gray before resizing
    cv::resize(cropped, cropped, {width, height}, cv::INTER_AREA);
    return cropped;
}


// Coordinates are on a doubled screenshot of the game without overscan.
// E.g., take Kega Fusion, File->Save Screenshot, and remove top and bottom overscan: 640x480->640x448 (16 lines from top and bottom)
cv::UMat cropGameRect(const cv::UMat& gameRect, int x, int y, int width, int height) {
    cv::Rect timeMatchRect = {
        static_cast<int>(std::round(x * gameRect.cols / 640.)),
        static_cast<int>(std::round(y * gameRect.rows / 448.)),
        static_cast<int>(std::round(width * gameRect.cols / 640.)),
        static_cast<int>(std::round(height * gameRect.rows / 448.)),
    };
    return gameRect(timeMatchRect);
}


void FrameAnalyzer::processLevelStart(const cv::UMat& gameRect, long long timestamp) {
    cv::UMat croppedRect, previousCroppedRect;
    bool previousLevelWithoutTimeBonus;
    {
        std::lock_guard guard(analysisMutex);
        bool currentLevelWithoutTimeBonus = splitWithoutTimeBonus(splitIndex, true);
        previousLevelWithoutTimeBonus = splitWithoutTimeBonus(splitIndex - 1, true);
        if (!currentLevelWithoutTimeBonus && !previousLevelWithoutTimeBonus) return;

        // In Sonic 1, level title is from ~1.27s to 3.36s after fade, Sonic 2: ~1.28s to 2.86s after fade
        if (!(lastSplitTime + 1800 <= timestamp && timestamp <= lastSplitTime + 2300)) return;
        if (timestamp - levelStartCroppedTimestamp < WAIT_AFTER_SPLIT_MS) return;
        if (settings.game == Game::Sonic1) {
            croppedRect = cropGameRect(gameRect, 340, 184, 142, 56);
        } else {
            croppedRect = cropGameRect(gameRect, 410, 112, 153, 32);
        }
        previousCroppedRect = levelStartCroppedRect;
        levelStartCroppedRect = croppedRect;
        levelStartCroppedTimestamp = timestamp;
    }

    if (previousLevelWithoutTimeBonus && !croppedRect.empty() && !previousCroppedRect.empty()) {
        double averageDifference = std::sqrt(
            cv::norm(croppedRect, previousCroppedRect, cv::NORM_L2SQR) / croppedRect.size().area() / croppedRect.channels());
        if (averageDifference < 10) {
            // Looks like it's the same level again. Undo split.
            callback.undoSplit();
        }
    }
}


bool FrameAnalyzer::detectRunFinish(const cv::UMat& gameRect, long long timestamp) const {
    int lastSplitIndex;
    if (settings.game == Game::Sonic1) {
        lastSplitIndex = 18;  // Final Zone
    } else {
        lastSplitIndex = 19;  // Death Egg Zone
    }
    {
        std::lock_guard lock(analysisMutex);
        if (splitIndex != lastSplitIndex) return false;
        if (timestamp - lastSplitTime < 10000) return false;
    }
    cv::UMat mask;
    if (settings.game == Game::Sonic1) {
        cv::inRange(gameRect, cv::Scalar(0, 0, 0), cv::Scalar(30, 30, 30), mask);  // Fade to black
    } else {
        cv::inRange(gameRect, cv::Scalar(180, 180, 180), cv::Scalar(255, 255, 255), mask);  // Fade to white
    }
    return maskNonZeroPart(mask) > 0.95;
}


FrameAnalyzer::OnFrameCapturedListenerImpl::OnFrameCapturedListenerImpl(FrameAnalyzer& frameAnalyzer) : frameAnalyzer(frameAnalyzer) {}


void FrameAnalyzer::OnFrameCapturedListenerImpl::onFrameCaptured(const VideoCaptureManager::CapturedFrame& capturedFrame) {
    std::lock_guard guard(frameAnalyzer.frameMutex);
    swap(frameAnalyzer.currentFrame, frameAnalyzer.previousFrame);
    frameAnalyzer.currentFrame = capturedFrame;
    frameAnalyzer.analysisThreadPool.enqueue_detach([this, currentFrame = frameAnalyzer.currentFrame, previousFrame = frameAnalyzer.previousFrame]() {
        frameAnalyzer.analyzeFrame(currentFrame, previousFrame);
    });
}


bool FrameAnalyzer::OnFrameCapturedListenerImpl::needsHighQualityResize() {
    std::lock_guard guard(frameAnalyzer.analysisMutex);
    TimeBonusState state = frameAnalyzer.timeBonusState;
    return state == TimeBonusState::CONFIRM_TIME_BONUS_LABEL ||
        state == TimeBonusState::CONFIRM_TIME_BONUS_POINTS ||
        state == TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START;
}

}  // namespace RTA
}  // namespace SonicVisualSplitBase
