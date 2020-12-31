#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <filesystem>

namespace SonicVisualSplitBase {

class DigitsRecognizer {
public:
	static DigitsRecognizer& getInstance(const std::string& gameName, const std::filesystem::path& templatesDirectory);

	std::vector<std::pair<cv::Rect2f, char>> findAllSymbolsLocations(cv::UMat frame, bool checkForScoreScreen);

	bool recalculatedDigitsPlacement();

	static void resetDigitsPlacement();

private:
	DigitsRecognizer(const std::string& gameName, const std::filesystem::path& templatesDirectory);

	std::vector<std::pair<cv::Rect2f, char>> removeOverlappingLocations(std::vector<std::tuple<cv::Rect2f, char, double>>& digitLocations);

	std::vector<std::pair<cv::Rect2f, double>> findSymbolLocations(cv::UMat frame, char symbol, std::filesystem::path templatesDirectory, bool bruteForceScale);

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

	static DigitsRecognizer* instance;

	// CONSTANTS
	// Similarity coefficient of the best match (more is better)
	static constexpr double MIN_SIMILARITY = -0.05;

	// minimum similarity in relation to the best found similarity
	static constexpr double SIMILARITY_COEFFICIENT = 2.25;

	// We search for symbols in our code
	static const char SCORE = 'S';
	static const char TIME = 'T';
};

}  // namespace SonicVisualSplitBase