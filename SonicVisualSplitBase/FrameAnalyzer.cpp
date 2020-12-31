#include "FrameAnalyzer.h"
#include "DigitsRecognizer.h"
#include "GameCapture.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>

namespace SonicVisualSplitBase {

// TODO
// make an "if" if stream preview size changes (not only the window itself)
// test stuff like closing, minimizing the window several times, etc
// check whether atexit works with winforms

// ===================
// options screen:
// Live preview
// Video settings:
// composite / rgb
// 4:3 / 16:9
// reset defaults
// ===================
// think about saving frames... okay analyzeresult contains "long captureTime" (ms from epoch); 
// if the time increases by more than the time passed (+1?), ignore that!!!!!!
// check how atexit is called
// fullscreen window is not positioned right. maybe look for client area, and which shift is proposed to it?
// if the time got down without the "could not recognize", ignore that
// double-check score (both the presense and the time). Split if they both match (presense and time)
// do something with split undos. On each split, calculate the sum of times!
// for the future: think about the end of a run; maybe detect "sega"? scaled down mb.

// Test EVERY option (including 16:9)
// set bestScale to -1 when the source is changed
// sometimes it fails???

const FrameAnalyzer& FrameAnalyzer::getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9) {
	if (instance == nullptr || instance->gameName != gameName || instance->templatesDirectory != templatesDirectory || instance->isStretchedTo16By9 != isStretchedTo16By9) {
		delete instance;
		instance = new FrameAnalyzer(gameName, templatesDirectory, isStretchedTo16By9);
	}
	return *instance;
}

FrameAnalyzer::FrameAnalyzer(const std::string& gameName, const std::filesystem::path& templatesDirectory, bool isStretchedTo16By9)
	: gameName(gameName), templatesDirectory(templatesDirectory), isStretchedTo16By9(isStretchedTo16By9) {}


AnalysisResult FrameAnalyzer::analyzeFrame(bool checkForScoreScreen, bool visualize, bool recalculateOnError) {
	result = AnalysisResult();
	result.foundAnyDigits = false;

	cv::UMat originalFrame = getGameFrame();
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

	std::vector<std::pair<cv::Rect2f, char>> allSymbols = findAllSymbolsLocations(frame, templatesDirectory, gameName, true);
	checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
	if (result.foundAnyDigits) {
		return result;
	}
	else {
		// we can still try to fix it - maybe video source properties changed!
		if (!recalculatedDigitsPlacement()) {
			allSymbols = findAllSymbolsLocations(frame, templatesDirectory, gameName, true);
			checkRecognizedSymbols(allSymbols, originalFrame, checkForScoreScreen, visualize);
		}
		if (!result.foundAnyDigits && visualize)
			visualizeResult(allSymbols);
		return result;
	}
}


void FrameAnalyzer::checkRecognizedSymbols(const std::vector<std::pair<cv::Rect2f, char>>& allSymbols, cv::UMat originalFrame, bool checkedForScoreScreen, bool visualize) {
	result.foundAnyDigits = false;
	std::map<char, std::vector<cv::Rect2f>> positionsOfSymbol;
	for (auto& [position, symbol] : allSymbols) {
		if (symbol == SCORE || symbol == TIME) {
			positionsOfSymbol[symbol].push_back(position);
		}
	}

	// score is divisible by 10, so there must be a zero on the screen
	if (positionsOfSymbol[SCORE].empty() || positionsOfSymbol[TIME].empty()) {
		result.errorReason = ErrorReasonEnum::NO_TIME_ON_SCREEN;
		return;
	}
	std::vector<cv::Rect2f>& timeRects = positionsOfSymbol[TIME];
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

	std::vector<std::pair<cv::Rect2f, char>> timeDigits;
	for (auto& [position, symbol] : allSymbols) {
		if (symbol != TIME && symbol != SCORE)
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

	// There's always a "TIME" label on top of the screen
	// But there's a second one during the score countdown screen ("TIME BONUS")
	result.isScoreScreen = (timeRects.size() >= 2);

	if (visualize) {
		auto recognizedSymbols = timeDigits;
		char additionalSymbols[] = {SCORE, TIME};
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
		cv::rectangle(result.visualizedFrame, position, cv::Scalar(0, 0, 255));
	}
}


bool FrameAnalyzer::checkIfFrameIsBlack(cv::UMat frame) {
	// calculating the median value out of 10'000 pixels, and checking if it's dark enough
	cv::Mat frameRead = frame.getMat(cv::ACCESS_READ);
	const int pixelsPerDimension = 100;
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
	//std::cout << "median: " << median << std::endl;
	return median <= 10;
}

}  // namespace SonicVisualSplitBase