#include "common.h"
#include "ffmpeg_wrap.h"
#include <iostream>

extern "C" {
#pragma warning(push)
#pragma warning(disable: 4819)
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#pragma warning(pop)
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")

std::thread FFmpegWrap::s_capture_thread;
bool FFmpegWrap::s_end_capture_thread = false;
std::vector<uint8_t> FFmpegWrap::s_buffer;
int FFmpegWrap::s_width = 0, FFmpegWrap::s_height = 0;
int FFmpegWrap::s_frame_index = 0;
std::mutex FFmpegWrap::s_mutex;

void FFmpegWrap::Init()
{
	avdevice_register_all();
}

std::vector<std::string> FFmpegWrap::ListCameras()
{
	const AVInputFormat* inputFormat = av_find_input_format("dshow");
	if (!inputFormat) {
		std::cerr << "Failed to find dshow input format dshow" << std::endl;
		return {};
	}

	AVDeviceInfoList* deviceList = nullptr;
	if (avdevice_list_input_sources(inputFormat, nullptr, nullptr, &deviceList) < 0) {
		std::cerr << "Failed to list input sources" << std::endl;
		return {};
	}

	std::vector<std::string> ret;
	for (int i = 0; i < deviceList->nb_devices; i++)
	{
		const AVDeviceInfo* device = deviceList->devices[i];
		bool is_video = false;
		for (int j = 0; j < device->nb_media_types; j++)
			if (device->media_types[j] == AVMEDIA_TYPE_VIDEO)
			{
				is_video = true;
				break;
			}
		if (is_video)
			ret.push_back(device->device_description);
	}

	avdevice_free_list_devices(&deviceList);

	return ret;
}

bool FFmpegWrap::CaptureCamera(const std::string& cam_name)
{
	AVFormatContext* inputFormatContext = NULL;
	const AVInputFormat* inputFormat = NULL;
	AVPacket* packet = NULL;
	AVFrame* frame = NULL;
	AVCodecContext* codecContext = NULL;
	const AVCodec* codec = NULL;
	int videoStreamIndex = -1;

	avdevice_register_all();

	// Find the input format
	inputFormat = av_find_input_format("dshow");
	if (!inputFormat) {
		std::cout << "Could not find input format dshow" << std::endl;
		return false;
	}

	// Open the input device
	int averror = avformat_open_input(&inputFormatContext, ("video=" + cam_name).c_str(), inputFormat, NULL);
	if (averror != 0) {
		char buf[1000];
		av_strerror(averror, buf, 1000);
		std::cout << "Could not open input device \"" << cam_name << "\": " << buf << std::endl;
		return false;
	}

	// Find the video stream
	if (avformat_find_stream_info(inputFormatContext, NULL) < 0) {
		std::cout << "Could not find stream information" << std::endl;
		return false;
	}
	for (uint32_t i = 0; i < inputFormatContext->nb_streams; i++) {
		if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStreamIndex = i;
			break;
		}
	}
	if (videoStreamIndex == -1) {
		std::cout << "Could not find video stream" << std::endl;
		return false;
	}

	// Get the codec parameters for the video stream
	codecContext = avcodec_alloc_context3(NULL);
	if (!codecContext) {
		std::cout << "Could not allocate codec context" << std::endl;
		return false;
	}
	if (avcodec_parameters_to_context(codecContext, inputFormatContext->streams[videoStreamIndex]->codecpar) < 0)
	{
		std::cout << "Could fill codec context" << std::endl;
		return false;
	}

	// Find the decoder for the codec
	codec = avcodec_find_decoder(codecContext->codec_id);
	if (!codec) {
		std::cout << "Codec " << codecContext->codec_id << "not found" << std::endl;
		return false;
	}

	// Open the codec
	if (avcodec_open2(codecContext, codec, NULL) < 0) {
		std::cout << "Could not open codec" << std::endl;
		return false;
	}

	// Allocate memory for the packet and frame
	packet = av_packet_alloc();
	if (!packet) {
		std::cout << "Could not allocate packet" << std::endl;
		return false;
	}
	frame = av_frame_alloc();
	if (!frame) {
		std::cout << "Could not allocate frame" << std::endl;
		return false;
	}

	constexpr int WIDTH = 1280;
	constexpr int HEIGHT = 720;
	struct SwsContext* swsContext = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt, WIDTH, HEIGHT, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
	if (!swsContext) {
		std::cout << "Could not create sws context" << std::endl;
		return false;
	}

	AVFrame* rgbFrame = av_frame_alloc();
	if (!rgbFrame) {
		std::cout << "Could not allocate RGB frame" << std::endl;
		return false;
	}

	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, WIDTH, HEIGHT, 1);
	s_buffer.resize(numBytes);
	av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, &s_buffer[0], AV_PIX_FMT_BGR24, WIDTH, HEIGHT, 1);

	s_height = HEIGHT;
	s_width = WIDTH;

	s_capture_thread = std::thread([videoStreamIndex, swsContext] (AVFormatContext *inputFormatContext, AVPacket *packet, AVCodecContext *codecContext, AVFrame *frame, AVFrame *rgbFrame){
		s_frame_index = 0;
		while (!s_end_capture_thread && av_read_frame(inputFormatContext, packet) >= 0) {
			if (packet->stream_index == videoStreamIndex) {
				int ret = avcodec_send_packet(codecContext, packet);
				if (ret < 0) {
					std::cout << "Error sending packet to decoder" << std::endl;
					break;
				}
				while (ret >= 0) {
					ret = avcodec_receive_frame(codecContext, frame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						break;
					}
					else if (ret < 0) {
						std::cout << "Error decoding frame" << std::endl;
						break;
					}

					// If we have a decoded frame, do something with it
					if (ret == 0) {
						std::lock_guard<std::mutex> lg(s_mutex);
						sws_scale(swsContext, (const uint8_t* const*)frame->data, frame->linesize, 0, codecContext->height, rgbFrame->data, rgbFrame->linesize);
						s_frame_index++;
					}
				}
			}

			av_packet_unref(packet);
		}

		sws_freeContext(swsContext);
		av_frame_free(&rgbFrame);
		av_packet_free(&packet);
		av_frame_free(&frame);
		avcodec_free_context(&codecContext);
		avformat_close_input(&inputFormatContext);
	}, inputFormatContext, packet, codecContext, frame, rgbFrame);

	return true;
}

int FFmpegWrap::GetLatestFrame(int lastFrame, cv::Mat &mat)
{
	if (lastFrame == s_frame_index)
		return lastFrame;
	if (mat.cols != s_width || mat.rows != s_height || mat.type() != CV_8UC3)
		mat = cv::Mat(s_height, s_width, CV_8UC3);
	std::lock_guard<std::mutex> lg(s_mutex);
	memcpy(mat.ptr(), &s_buffer[0], s_height * s_width * 3);
	return s_frame_index;
}

void FFmpegWrap::StopCapture()
{
	s_end_capture_thread = true;
	s_capture_thread.join();
}