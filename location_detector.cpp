#include "location_detector.h"

// Pre-process the location names to make matching easier
[[nodiscard]]
static std::string PreprocessLocationName(const std::string& loc_in)
{
	std::string ret = loc_in;
	// spaces and single quotes are ignored since sometimes they are not correctly recognized
	ret.erase(std::remove_if(ret.begin(), ret.end(), [](auto& c) -> bool { return c == ' ' || c == '\''; }), ret.end());

	// The Hylia Serif font (unofficial name) used by BotW has similar glyphs for upper/lower-case letters, causing the recognition to mix them sometimes.
	// Forcing to uppercase to get rid of this confusion.
	std::for_each(ret.begin(), ret.end(), [](auto& c) { c = std::toupper(c); });

	return ret;
}

bool LocationDetector::Init(const char* lang, int brightness_threshold, int bright_pixel_ratio_low, int bright_pixel_ratio_high)
{
	if (_tess_api.Init(".", lang))
	{
		std::cout << "OCRTesseract: Could not initialize tesseract." << std::endl;
		return false;
	}

	// locations are always on one line
	_tess_api.SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_LINE);

	// these are the possible characters in location names in the English version of the game
	if (std::string_view(lang) == "eng")
	{
		if (!_tess_api.SetVariable("tessedit_char_whitelist", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'- "))
			return false;
	}
	// ignore extra space at the end of the line without any text, doesn't seem to make much difference though
	if (!_tess_api.SetVariable("gapmap_use_ends", "true"))
		return false;

	if (!InitLocationList(lang))
		return false;

	_brightness_threshold = brightness_threshold;
	_bright_pixel_ratio_low = bright_pixel_ratio_low / 100.0;
	_bright_pixel_ratio_high = bright_pixel_ratio_high / 100.0;

	return true;
}

bool LocationDetector::InitLocationList(const char* lang)
{
	std::string shrine_list_file(std::string(lang) + "_locations.txt");
	std::ifstream ifs(shrine_list_file);
	if (!ifs.is_open())
	{
		std::cout << "Cannot open file " << shrine_list_file << std::endl;
		return false;
	}

	std::string line;
	while (std::getline(ifs, line))
		_locations.emplace_back(line, PreprocessLocationName(line));

	return true;
}

bool LocationDetector::EarlyOutTest(const cv::Mat& game_img, uint32_t location_col0, uint32_t location_col1, uint32_t location_row0, uint32_t location_row1)
{
	// Peek the left-most quarter of the location frame, the shorted location name is "Docks", which is about this wide
	cv::Mat locationMinimalFrame;
	cv::cvtColor(game_img(cv::Rect(location_col0, location_row0, (location_col1 - location_col0) / 4, location_row1 - location_row0)), locationMinimalFrame, cv::COLOR_BGR2GRAY);		// converting to gray
	// scan this area for bright pixels.
	{
		uint32_t num_bright_pixel = 0;
		for (int i = 0; i < locationMinimalFrame.rows; i++)
		{
			uint8_t* data = locationMinimalFrame.row(i).data;
			for (int j = 0; j < locationMinimalFrame.cols; j++)
				if (data[j] > _brightness_threshold)
					num_bright_pixel++;
		}
		double bright_pixel_ratio = double(num_bright_pixel) / (locationMinimalFrame.rows * locationMinimalFrame.cols);
		if (bright_pixel_ratio < _bright_pixel_ratio_low || bright_pixel_ratio > _bright_pixel_ratio_high)
			return true;
	}

	return false;
}

std::string LocationDetector::FindBestLocationMatch(const std::string& loc_in)
{
	std::string loc_in_preprocessed = PreprocessLocationName(loc_in);
	uint32_t max_allowed_edits = uint32_t(loc_in_preprocessed.size() / 5);			// allow maximum 1/5 recognition error
	uint32_t candidate_num_edits = max_allowed_edits + 1;
	std::string candidate;
	for (const Location& loc : _locations)
	{
		if (uint32_t(abs(int32_t(loc.preprocessed_name.size()) - int32_t(loc_in_preprocessed.size()))) > max_allowed_edits)
			continue;

		uint32_t num_edits = util::GetStringEditDistance(loc.preprocessed_name, loc_in_preprocessed, max_allowed_edits + 1);
		// prefer shorter names if the editing distance is the same.
		// This is because Tesseract might incorrectly recognize extra random characters inside the location bbox but after the names.
		if (num_edits < candidate_num_edits || (num_edits == candidate_num_edits && candidate.size() > loc.name.size()))
		{
			candidate_num_edits = num_edits;
			candidate = loc.name;
		}
	}
	return candidate;
}

std::string LocationDetector::GetLocation(const cv::Mat& game_img)
{
	// This is the bounding box of the longest location text in the lower left corner of the game screen.
	constexpr double location_x0 = 0.038461538461;
	constexpr double location_x1 = 0.502652519893;
	constexpr double location_y0 = 0.838443396226;
	constexpr double location_y1 = 0.926886792452;

	// get the bounding box in rows / cols
	uint32_t location_col0 = uint32_t(location_x0 * double(game_img.cols) + 0.5);
	uint32_t location_col1 = uint32_t(location_x1 * double(game_img.cols) + 0.5);
	uint32_t location_row0 = uint32_t(location_y0 * double(game_img.rows) + 0.5);
	uint32_t location_row1 = uint32_t(location_y1 * double(game_img.rows) + 0.5);

	if (EarlyOutTest(game_img, location_col0, location_col1, location_row0, location_row1))
		return "";

	// shrink the whole location frame to make OCR faster
	cv::Mat location_frame;
	double scale_factor = std::max(game_img.cols / 480.0, 1.0);	// according to experiments, it's still possible to recognize the location with high accuracy when the width of the game screen is 480.
	cv::resize(game_img(cv::Rect(location_col0, location_row0, location_col1 - location_col0, location_row1 - location_row0)), location_frame, cv::Size(int((location_col1 - location_col0) / scale_factor), int((location_row1 - location_row0) / scale_factor)));

	cv::cvtColor(location_frame, location_frame, cv::COLOR_BGR2GRAY);
	for (int i = 0; i < location_frame.rows; i++)
	{
		uint8_t* data = location_frame.row(i).data;
		for (int j = 0; j < location_frame.cols; j++)
			data[j] = 255 - (std::max(data[j], uint8_t(204)) - 204) * 5;		// invert the image so that the text is black-on-white. For some reason, Tesseract OCRs such text at almost double the speed compared to white-on-black text.
	}
	cv::cvtColor(location_frame, location_frame, cv::COLOR_GRAY2BGRA);
	//cv::cvtColor(location_frame, location_frame, cv::COLOR_BGR2BGRA);

	//imwrite("test.png", location_frame);
	// convert to BGRA so that each pixel has 4 bytes (needed because opencv uses little-endian in Mat and leptonica uses big-endian in PIX)
	//location_frame = cv::imread("test.png");
	//cv::cvtColor(location_frame, location_frame, cv::COLOR_BGR2BGRA);

	// reorder the channels in each pixel for leptonica
	util::OpenCvMatBGRAToLeptonicaRGBAInplace(location_frame);

	// construct the PIX struct
	PIX pix;
	memset(&pix, 0, sizeof(pix));
	pixSetDimensions(&pix, location_frame.cols, location_frame.rows, 32);
	pixSetWpl(&pix, location_frame.cols);
	pixSetSpp(&pix, 4);
	pix.refcount = 1;
	pix.informat = IFF_UNKNOWN;
	pixSetData(&pix, (l_uint32*)location_frame.data);

	// OCR
	_tess_api.SetImage(&pix);
	_tess_api.Recognize(0);

	// some post-process
	{
		std::string boxes = std::unique_ptr<char[]>(_tess_api.GetBoxText(0)).get();
		std::istringstream boxesstream(boxes);
		std::string letter;
		int letter_x0, letter_y0, letter_x1, letter_y1;
		if (!(boxesstream >> letter >> letter_x0 >> letter_y0 >> letter_x1 >> letter_y1))
			return "";
		if (letter_x0 > location_frame.rows / 2)		// text not starting from the left side of the location frame, one possibility is that dialog text is recognized (right side of the location bounding-box overlaps with the dialog box)
			return "";
		if (letter_x1 - letter_x0 > location_frame.rows)	// text bounding box is weird-shaped
			return "";
	}

	std::string ret = std::unique_ptr<char[]>(_tess_api.GetUTF8Text()).get();

	// OCR text from tesseract sometimes ends with '\n', trim that
	if (ret[ret.size() - 1] == '\n')
		ret = ret.substr(0, ret.size() - 1);

	return FindBestLocationMatch(ret);
}

LocationDetector::~LocationDetector()
{
	_tess_api.Clear();
}