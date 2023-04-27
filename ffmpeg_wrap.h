#pragma once

#include <string>
#include <vector>
#include <thread>


class FFmpegWrap
{
private:
	static std::thread s_capture_thread;
	static bool s_end_capture_thread;
	static std::vector<uint8_t> s_buffer;
	static int s_width, s_height;
	static int s_frame_index;
	static std::mutex s_mutex;
public:
	static void Init();
	static std::vector<std::string> ListCameras();
	static bool CaptureCamera(const std::string& cam_name);
	static int GetLatestFrame(int lastFrame, cv::Mat &mat);
	static void StopCapture();
};