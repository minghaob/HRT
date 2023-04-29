#include "common.h"
#include "location_detector.h"
#include "ffmpeg_wrap.h"
#include "server.h"

//cv::Rect gameRect(412, 114, 1920 - 412, 962 - 114);

Server g_server;

void AnalyseVideo(const std::string &video_file, cv::Rect game_rect, int frame_start, int frame_length, const std::string &output_file, int brightness_threshold, int bright_pixel_ratio_low, int bright_pixel_ratio_high)
{
	std::string lang = "eng";

	LocationDetector location_detector;
	if (!location_detector.Init(lang.c_str(), brightness_threshold, bright_pixel_ratio_low, bright_pixel_ratio_high))
		return;

	std::ofstream ofs;
	if (output_file.size())
	{
		ofs.open(output_file);
		if (!ofs.is_open())
			std::cout << "Cannot open output file " << output_file << ". Result will not be output to file" << std::endl;
	}

	cv::VideoCapture cap(video_file);
	if (cap.isOpened())
	{
		double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
		double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
		std::cout << "File: " << video_file << std::endl;
		std::cout << "Frame size: " << width << "x" << height << std::endl;
		double num_frames = cap.get(cv::CAP_PROP_FRAME_COUNT);
		std::cout << "Number of frames: " << (int)num_frames << std::endl;

		// Get the frame rate of the video
		double fps = cap.get(cv::CAP_PROP_FPS);
		std::cout << "Frame rate: " << fps << std::endl;
		double duration_sec = num_frames / fps;
		std::cout << "Duration: " << duration_sec << " seconds" << std::endl;

		double format = cap.get(cv::CAP_PROP_FORMAT);
		std::cout << "Format: " << format << std::endl;

		if (frame_start < 0 || frame_start >= num_frames)
			return;
		if (frame_length < 0)
			frame_length = (int)num_frames;

		if (frame_start > 0)
			cap.set(cv::CAP_PROP_POS_FRAMES, frame_start - 1);			// VideoCapture.read() reads the next frame

		if (game_rect.width <= 0 || game_rect.height <= 0)
		{
			game_rect.x = 0;
			game_rect.y = 0;
			game_rect.width = (int)width;
			game_rect.height = (int)height;
		}

		if (game_rect.x < 0 || game_rect.y < 0 || game_rect.x + game_rect.width >(int)width || game_rect.y + game_rect.height >(int)height)
		{
			std::cout << "Error: game image area outside video frame" << std::endl;
			return;
		}
		std::cout << "Game area: (" << game_rect.x << ", " << game_rect.y << ") + (" << game_rect.width << ", " << game_rect.height << ")" << std::endl;

		for (int32_t frame_number = frame_start; frame_number < frame_start + frame_length; frame_number++)
		{
			DWORD tbegin = ::timeGetTime();
			cv::Mat frame;
			if (!cap.read(frame))
				break;

			int cur_frame = int(cap.get(cv::CAP_PROP_POS_FRAMES));
			char buf[30];
			{
				double sec_lf;
				int frame_in_sec = int(std::modf(cur_frame / fps, &sec_lf) * fps + 0.5);
				int sec = int(sec_lf);
				sprintf_s(buf, "[%6d] %02d:%02d:%02d.%02d", cur_frame, sec / 3600, sec % 3600 / 60, sec % 60 , frame_in_sec);
			}

			g_server.SetLastImage(frame(game_rect));
			std::string location = location_detector.GetLocation(frame(game_rect));
			DWORD tend = ::timeGetTime();
			if (location.size() > 0)
			{
				if (location[location.size() - 1] == '\n')
					location = location.substr(0, location.size() - 1);
				g_server.PushMessage(location);
				std::ostringstream os;
				os << buf << ": " << location;

				if (os.str().length() < 60)
					os << std::string(60 - os.str().length(), ' ');
				os << tend - tbegin << "ms";

				std::cout << os.str() << "  \r";
				if (ofs.is_open())
					ofs << os.str() << std::endl;
			}
			else
			{
				if (cur_frame % 30 == 0)
					std::cout << buf << std::string(70 - strlen(buf), ' ') << '\r';
			}
		}
	}
	else
	{
		std::cout << "Cannot open video file " << video_file << std::endl;
	}
}

void AnalyseLiveStream(int brightness_threshold, int bright_pixel_ratio_low, int bright_pixel_ratio_high)
{
	std::string lang = "eng";

	LocationDetector location_detector;
	if (!location_detector.Init(lang.c_str(), brightness_threshold, bright_pixel_ratio_low, bright_pixel_ratio_high))
		return;

	int last_frame = -1;
	cv::Mat mat;
	while (1)
	{
		DWORD tbegin = ::timeGetTime();
		int cur_frame = FFmpegWrap::GetLatestFrame(last_frame, mat);
		if (cur_frame != last_frame)
		{
			char buf[50];
			{
				auto now = std::chrono::system_clock::now();
				auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
				auto diff = now_ms.time_since_epoch();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff) % 1000;

				std::time_t time = std::chrono::system_clock::to_time_t(now);
				std::ostringstream os;
#pragma warning(push)
#pragma warning(disable: 4996)
				os << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
#pragma warning(pop)

				sprintf_s(buf, "[%6d] %s", cur_frame, os.str().c_str());
			}

			g_server.SetLastImage(mat);
			std::string location = location_detector.GetLocation(mat);
			DWORD tend = ::timeGetTime();
			if (location.size() > 0)
			{
				if (location[location.size() - 1] == '\n')
					location = location.substr(0, location.size() - 1);
				g_server.PushMessage(location);
				std::ostringstream os;
				os << buf << ": " << location;

				if (os.str().length() < 60)
					os << std::string(60 - os.str().length(), ' ');
				os << tend - tbegin << "ms";

				os << '\r';

				std::cout << os.str();
			}
			else
				std::cout << buf << std::string(70 - strlen(buf), ' ') << '\r';

			last_frame = cur_frame;
		}
		else
		{
			::Sleep(1);
		}
	}
}

void DisplayHelpText()
{
	std::cout << "Options:" << std::endl;
	std::cout << "  -v file start length      take a video file as input" << std::endl;
	std::cout << "                            start and length specifies the frame range" << std::endl;
	std::cout << "  -b x y w h                specify the area of the game image" << std::endl;
	std::cout << "                            (if not specified, the whole input image is used)" << std::endl;
	std::cout << "  -e threshold ratio_low ratio_high" << std::endl;
	std::cout << "                            specify the early out parameters" << std::endl;
	std::cout << "                            if percentage of pixels in the location box that are brighter than" << std::endl;
	std::cout << "                            threshold is not in [ratio_low, ratio_high], OCR will be skipped." << std::endl;
	std::cout << "                            Brightness in range 0-255, ratios are in percentage." << std::endl;
	std::cout << "                            Default values are 240 15 30." << std::endl;
	std::cout << "  -o output_file            output detected locations with timestamp to a file" << std::endl;
}

bool str_to_int(const std::string& in_str, int& out_int)
{
	std::size_t pos;
	out_int = std::stoi(in_str, &pos);
	return pos == in_str.size();
}

int main(int argc, char* argv[])
{
	// just in case of non-ansi text in console
	::SetConsoleOutputCP(CP_UTF8);

	// for shorter thread time slices
	::timeBeginPeriod(1);

	// disable sleep mode
	::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

	FFmpegWrap::Init();

	bool video_mode = false;
	int frame_start = 0, num_frame = -1;
	std::string video_file_name;
	int bbox_x = 0, bbox_y = 0, bbox_w = 0, bbox_h = 0;
	std::string output_file_name;

	// location text has brightness of 245+. Use a loose threshold here to account for blur / compression loss or any filter that camera might apply
	// This is a conservative range, usually it's around 18% - 25%
	int brightness_threshold = 240, bright_pixel_ratio_low = 15, bright_pixel_ratio_high = 30;

	for (int i = 1; i < argc; i++)
	{
		std::string_view cur_arg(argv[i]);
		if (cur_arg == "-v")
		{
			video_mode = true;
			if (argc <= i + 3)
			{
				DisplayHelpText();
				return 0;
			}
			video_file_name = argv[i + 1];
			if (!str_to_int(argv[i + 2], frame_start) || !str_to_int(argv[i + 3], num_frame))
			{
				DisplayHelpText();
				return 0;
			}
			i += 3;
		}
		else if (cur_arg == "-b")
		{
			if (argc <= i + 4)
			{
				DisplayHelpText();
				return 0;
			}
			if (!str_to_int(argv[i + 1], bbox_x) || !str_to_int(argv[i + 2], bbox_y) || !str_to_int(argv[i + 3], bbox_w) || !str_to_int(argv[i + 4], bbox_h))
			{
				DisplayHelpText();
				return 0;
			}
			i += 4;
		}
		else if (cur_arg == "-e")
		{
			if (argc <= i + 3)
			{
				DisplayHelpText();
				return 0;
			}
			if (!str_to_int(argv[i + 1], brightness_threshold) || !str_to_int(argv[i + 2], bright_pixel_ratio_low) || !str_to_int(argv[i + 3], bright_pixel_ratio_high))
			{
				DisplayHelpText();
				return 0;
			}
			i += 3;
		}
		else if (cur_arg == "-o")
		{
			if (argc <= i + 1)
			{
				DisplayHelpText();
				return 0;
			}
			output_file_name = argv[i + 1];
			i += 1;
		}
		else
		{
			DisplayHelpText();
			return 0;
		}
	}

	if (video_mode)
	{
		if (!g_server.Start())
			return 0;

		std::cout << "Running in video file mode" << std::endl;

		HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
		::SetConsoleTextAttribute(hConsole, 10);
		std::cout << "Run \"webui.bat\" to start the web-ui" << std::endl;
		::SetConsoleTextAttribute(hConsole, 7);

		AnalyseVideo(video_file_name, cv::Rect(bbox_x, bbox_y, bbox_w, bbox_h), frame_start, num_frame, output_file_name, brightness_threshold, bright_pixel_ratio_low, bright_pixel_ratio_high);
	}
	else
	{
		std::vector<std::string> cams = FFmpegWrap::ListCameras();
		if (cams.size() == 0)
		{
			std::cout << "No cameras found." << std::endl;
			return 0;
		}

		std::cout << "Found " << cams.size() << " cameras" << std::endl;

		for (int i = 0; i < int(cams.size()); i++)
			std::cout << "[" << i + 1 << "]: " << cams[i] << std::endl;
		std::cout << "Choose your input stream (1-" << cams.size() << "): ";
		std::string input;
		std::cin >> input;
		int choice = -1;
		if (!str_to_int(input, choice) || choice <=0 || choice > int(cams.size()))
		{
			std::cout << "Invalid input, please enter a number in the given range" << std::endl;
			return 0;
		}

		if (!FFmpegWrap::CaptureCamera(cams[choice - 1]))
		{
			std::cout << "Failed to capture camera." << std::endl;
			return 0;
		}

		if (!g_server.Start())
			return 0;

		HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
		::SetConsoleTextAttribute(hConsole, 10);
		std::cout << "Run \"webui.bat\" to start the web-ui" << std::endl;
		::SetConsoleTextAttribute(hConsole, 7);

		AnalyseLiveStream(brightness_threshold, bright_pixel_ratio_low, bright_pixel_ratio_high);

		FFmpegWrap::StopCapture();
	}

	g_server.Stop();

	return 0;
}
