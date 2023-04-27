#include "common.h"


class LocationDetector
{
private:
	struct Location
	{
		std::string name;
		std::string preprocessed_name;
	};

private:
	tesseract::TessBaseAPI _tess_api;
	std::vector<Location> _locations;
	int _brightness_threshold = 240;
	double _bright_pixel_ratio_low = 0.15, _bright_pixel_ratio_high = 0.3;

private:
	bool InitLocationList(const char* lang);

	// returns true if this image should be early-outed, i.e. it's not likely it has a location in the image
	bool EarlyOutTest(const cv::Mat& game_img, uint32_t location_col0, uint32_t location_col1, uint32_t location_row0, uint32_t location_row1);

	// Lookup the location list and find the best match for the detected location string
	std::string FindBestLocationMatch(const std::string& loc_in);

public:
	LocationDetector() = default;
	~LocationDetector();
	bool Init(const char* lang, int brightness_threshold, int bright_pixel_ratio_low, int bright_pixel_ratio_high);

	// returns empty string if nothing is detected
	std::string GetLocation(const cv::Mat& game_img);
};