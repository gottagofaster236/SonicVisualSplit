#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <filesystem>

namespace SonicVisualSplitBase {

class DigitsRecognizer {
public:
	static DigitsRecognizer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory);

	// Find locations of all digits, "SCORE" and "TIME" labels.
	std::vector<std::pair<cv::Rect2f, char>> findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen);

	// We precalculate the rectangle where all of the digits are located.
	// In case of error (e.g. video source properties changed), we may want to recalculate that.
	static void resetDigitsPlacement();

	bool recalculatedDigitsPlacementLastTime();

	// We search for symbols in our code
	static const char SCORE = 'S';
	static const char TIME = 'T';

private:
	DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory);

	std::vector<std::pair<cv::Rect2f, double>> findSymbolLocations(cv::UMat frame, char symbol, bool recalculateDigitsPlacement);

	std::vector<std::pair<cv::Rect2f, char>> removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations);

	std::tuple<cv::UMat, cv::UMat, int> loadImageAndMaskFromFile(char symbol);

	std::string gameName;
	std::filesystem::path templatesDirectory;

	// Scale of the image which matches the templates (i.e. digits) the best. -1, if not calculated yet.
	double bestScale = -1;

	// The region of interest (ROI) where the time digits are located (i.e. we don't search the whole frame)
	cv::Rect digitsRoi;

	// Similarity coefficient of the best match
	double bestSimilarity;

	bool recalculateDigitsPlacement;

	// map: symbol (a digit, TIME or SCORE) -> {image of the symbol, binary alpha mask, count of opaque pixels}
	std::map<char, std::tuple<cv::UMat, cv::UMat, int>> templates;

	inline static DigitsRecognizer* instance = nullptr;

	// CONSTANTS
	// Minimum similarity of a match (zero is a perfect match)
	static constexpr double MIN_SIMILARITY = -0.05;

	// minimum similarity in relation to the best found similarity
	static constexpr double SIMILARITY_COEFFICIENT = 2.25;
};

}  // namespace SonicVisualSplitBase