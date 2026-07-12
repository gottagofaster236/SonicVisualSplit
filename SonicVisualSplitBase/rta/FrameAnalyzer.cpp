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
constexpr long long TIME_BONUS_CHECK_PERIOD_MS = 500;
constexpr double MIN_BRIGHTNESS_DECREASE_PART = 0.6;
constexpr double MAX_BRIGHTNESS_INCREASE_PART = 0.15;

using VideoCaptureManager::CapturedFrame;

FrameAnalyzer::FrameAnalyzer(const AnalysisSettings& settings, TimerCallback& callback) :
    settings(settings), callback(callback), onFrameCapturedListener(*this), 
    templateMatcher(settings, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", TemplateMatcher::TIME}) {
    VideoCaptureManager::addOnFrameCapturedListener(onFrameCapturedListener);
}


FrameAnalyzer::~FrameAnalyzer() {
    VideoCaptureManager::removeOnFrameCapturedListener(onFrameCapturedListener);
    analysisThreadPool.clear_tasks();
    analysisThreadPool.wait_for_tasks();
}


AnalysisResult FrameAnalyzer::getLastAnalysisResult() {
    AnalysisResult result;

    CapturedFrame currentFrame;
    {
        std::lock_guard guard(frameMutex);
        currentFrame = this->currentFrame;
    }
    if (currentFrame.frame.empty() || VideoCaptureManager::getCurrentTimeInMilliseconds() - currentFrame.timestamp > 5000) {
        result.errorReason = ErrorReasonEnum::VIDEO_DISCONNECTED;
    }

    cv::Rect gameRect;
    {
        std::lock_guard guard(analysisMutex);
        gameRect = this->gameRect;
    }

    currentFrame.frame.copyTo(result.visualizedFrame);
    if (!gameRect.empty()) {
        int lineThickness = 1 + result.visualizedFrame.rows / 500;
        cv::rectangle(result.visualizedFrame, gameRect, cv::Scalar(255, 0, 255), lineThickness);
    } else if (result.errorReason == ErrorReasonEnum::NO_ERROR) {
        result.errorReason = ErrorReasonEnum::NO_GAME_RECT;
    }

    if (!isSupportedGame()) {
        result.errorReason = ErrorReasonEnum::UNSUPPORTED_GAME;
    }

    return result;
}


bool FrameAnalyzer::isSupportedGame() const {
    return settings.game == Game::Sonic1 || settings.game == Game::Sonic2;
}

bool FrameAnalyzer::splitWithoutTimeBonus(int splitIndex) const {
    if (settings.game == Game::Sonic1 && (splitIndex == 17 || splitIndex == 18))
    {
        // Sonic 1's Scrap Brain 3 doesn't have the proper transition. Also, Final Zone just fades to black normally.
        return true;
    } else if (settings.game == Game::Sonic2 && (splitIndex == 17 || splitIndex == 18))
    {
        // Same for Sonic 2's Sky Chase and Wing Fortress.
        return true;
    }
    return false;
}


void FrameAnalyzer::reportSplitIndex(int splitIndex) {
    std::lock_guard guard(analysisMutex);
    if (this->splitIndex == splitIndex) return;
    this->splitIndex = splitIndex;
    timeBonusState = TimeBonusState::INITIAL;
}


static bool detectFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame, const cv::Rect& gameRect);
static bool isTitleScreen(const cv::UMat& gameScreen);


void FrameAnalyzer::analyzeFrame(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame) {
    if (!isSupportedGame()) return;
    if (currentFrame.frame.empty() || previousFrame.frame.empty() || currentFrame.frame.size != previousFrame.frame.size) {
        return;
    }

    bool timerStarted;
    {
        std::lock_guard guard(analysisMutex);
        timerStarted = splitIndex != -1;
    }

    if (!timerStarted) {
        cv::Rect gameRectOnFade = detectGameRectOnFade(currentFrame, previousFrame);
        if (gameRectOnFade.empty()) return;
        detectGameRectOnSegaScreen(previousFrame, gameRectOnFade);
    }

    cv::Rect gameRect;
    {
        std::lock_guard guard(analysisMutex);
        if (this->gameRect.empty() || gameRectFrameSize != previousFrame.frame.size()) return;
        gameRect = this->gameRect;
        timerStarted = splitIndex != -1;
    }

    bool detectedFade = detectFade(currentFrame, previousFrame, gameRect);
    bool detectedRunFinish = !detectedFade && detectRunFinish(currentFrame.frame(gameRect));
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
            if (timerStarted && !detectedRunFinish && 
                timeBonusState != TimeBonusState::AFTER_TIME_BONUS && !splitWithoutTimeBonus(splitIndex)) return;
            lastSplitTime = currentFrame.timestamp;
            splitIndex++;
            timeBonusState = TimeBonusState::INITIAL;
        }

        if (!timerStarted) {
            callback.startTimer();
        } else {
            callback.split();
        }
    }

    processTimeBonus(currentFrame.frame(gameRect), currentFrame.timestamp);
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

    if (maskNonZeroPart(decreasedBrightnessMask(gameRect)) < MIN_BRIGHTNESS_DECREASE_PART) return {};
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
        cv::inRange(oldGameScreen, cv::Scalar(170, 60, 0), cv::Scalar(255, 130, 30), blueColorMask);
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


bool detectFade(const CapturedFrame& currentFrame, const CapturedFrame& previousFrame, const cv::Rect& gameRect) {
    cv::UMat currentScreen = currentFrame.frame(gameRect);
    cv::UMat previousScreen = previousFrame.frame(gameRect);
    cv::UMat decreasedBrightnessMask = calculateIncreasedBrightnessMask(previousScreen, currentScreen);
    if (maskNonZeroPart(decreasedBrightnessMask) < MIN_BRIGHTNESS_DECREASE_PART) return false;
    cv::UMat increasedBrightnessMask = calculateIncreasedBrightnessMask(currentScreen, previousScreen);
    return maskNonZeroPart(increasedBrightnessMask) < MAX_BRIGHTNESS_INCREASE_PART;
}

bool isTitleScreen(const cv::UMat& gameScreen) {
    // Both Sonic 1 and 2 have red backgrounds behind letters on the title screen.
    cv::UMat redColorMask;
    cv::inRange(gameScreen, cv::Scalar(0, 0, 80), cv::Scalar(30, 30, 240), redColorMask);
    return maskNonZeroPart(redColorMask) > 0.015;
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
        bool timeBonusStringChanged;
        {
            std::lock_guard guard(analysisMutex);
            timeBonusStringChanged = timeBonusString != lastTimeBonusString;
        }
        {
            std::lock_guard guard(analysisMutex);
            if (!foundTimeBonusPoints || (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_POINTS && timeBonusStringChanged)) {
                this->timeBonusState = TimeBonusState::INITIAL;
            } else if (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_LABEL) {
                this->timeBonusState = TimeBonusState::CONFIRM_TIME_BONUS_POINTS;
            } else if (this->timeBonusState == TimeBonusState::CONFIRM_TIME_BONUS_POINTS) {
                this->timeBonusState = TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START;
            }
        }
        break;
    }
    case TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START:
    {
        TemplateMatcher::Match digitMatchToObserve;
        {
            std::lock_guard guard(analysisMutex);
            digitMatchToObserve = this->digitMatchToObserve;
        }
        if (templateMatcher.getNewSimilarity(cropToDigitsRect(gameRect), digitMatchToObserve) < 1.1 * digitMatchToObserve.similarity) {
            {
                std::lock_guard guard(analysisMutex);
                if (this->timeBonusState != TimeBonusState::WAIT_FOR_COUNTDOWN_TO_START) break;
                this->timeBonusState = TimeBonusState::PAUSED_FOR_TIME_BONUS;
            }
            pauseForTimeBonus(timeBonusPoints);
            {
                std::lock_guard guard(analysisMutex);
                if (this->timeBonusState == TimeBonusState::PAUSED_FOR_TIME_BONUS) {
                    this->timeBonusState = TimeBonusState::AFTER_TIME_BONUS;
                }
            }
        }
        break;
    }
    default: {}
    }
}


static cv::UMat cropGameRectRelative(const cv::UMat& src, int x, int y, int width, int height, bool filterYellowColor);


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
            std::lock_guard guard(analysisMutex);
            this->timeBonusPoints = std::stoi(possibleTimeBonus);
            this->timeBonusString = timeBonusString;
            this->digitMatchToObserve = matches[timeBonusString.size() - possibleTimeBonus.size()];
            return true;
        }
    }
    return false;
}


cv::UMat FrameAnalyzer::cropToDigitsRect(const cv::UMat& gameRect) const {
    if (settings.game == Game::Sonic1) {
        return cropGameRectRelative(gameRect, 382, 230, 102, 30, false);
    } else {
        return cropGameRectRelative(gameRect, 412, 222, 104, 30, false);
    }
}


bool FrameAnalyzer::hasTimeBonus(const cv::UMat& gameRect) const {
    cv::UMat timeMatchRect;
    if (settings.game == Game::Sonic1) {
        timeMatchRect = cropGameRectRelative(gameRect, 158, 230, 70, 30, true);
    } else {
        timeMatchRect = cropGameRectRelative(gameRect, 134, 222, 68, 30, true);
    }
    return !templateMatcher.findTemplateLocations(timeMatchRect, {TemplateMatcher::TIME}, false).empty();
}


void FrameAnalyzer::pauseForTimeBonus(int timeBonusPoints) {
    int framesToPause = timeBonusPoints / 100;
    if (settings.game == Game::Sonic2 && timeBonusPoints >= 1000) {
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


cv::UMat cropGameRectRelative(const cv::UMat& gameRect, int x, int y, int width, int height, bool filterYellowColor) {
    cv::Rect timeMatchRect = {
        static_cast<int>(std::round(x * gameRect.cols / 640.)),
        static_cast<int>(std::round(y * gameRect.rows / 448.)),
        static_cast<int>(std::round(width * gameRect.cols / 640.)),
        static_cast<int>(std::round(height * gameRect.rows / 448.)),
    };
    cv::UMat cropped = gameRect(timeMatchRect);
    cropped = TemplateMatcher::convertToGray(cropped, filterYellowColor);  // Convert to gray before resizing
    cv::resize(cropped, cropped, {width, height}, cv::INTER_AREA);
    return cropped;
}


bool FrameAnalyzer::detectRunFinish(const cv::UMat& gameRect) const {
    if (settings.game != Game::Sonic2) return false;  // Sonic 1 has a usual fade, no need for special detection.
    {
        std::lock_guard lock(analysisMutex);
        // Death Egg Zone
        if (splitIndex != 19) return false;
    }
    cv::UMat whiteColorMask;
    cv::inRange(gameRect, cv::Scalar(180, 180, 180), cv::Scalar(255, 255, 255), whiteColorMask);
    return maskNonZeroPart(whiteColorMask) > 0.7;
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
