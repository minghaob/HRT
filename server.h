#pragma once
#include <vector>
#include <fstream>
#include <set>
#include <memory>
#include <shared_mutex>
#define ASIO_STANDALONE 1
#include "Simple-Web-Server/server_http.hpp"
#pragma warning(push)
#pragma warning(disable:4267)
#include "Simple-WebSocket-Server/server_ws.hpp"
#include "Simple-WebSocket-Server/client_ws.hpp"
#pragma warning(pop)

class Server
{
	using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
	using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

	// http server
	HttpServer _http_server;
	std::thread _http_server_thread;

	// ws server
	std::set<std::shared_ptr<WsServer::Connection>> _ws_connections;
	std::shared_mutex _ws_connection_mutex;
	WsServer _ws_server;
	std::thread _ws_server_thread;

	// push
	std::thread _broadcast_thread;
	std::mutex _push_mutex;
	std::condition_variable _push_cv;
	std::string _broadcast_str;

	cv::Mat _last_image;
	std::mutex _last_image_mutex;
	std::vector<uint8_t> _last_image_jpg;
	bool _last_image_jpg_valid = false;

	bool _is_running = false;

public:
	bool Start();
	void Stop();
	void PushMessage(const std::string &msg);
	void SetLastImage(cv::Mat img);
};