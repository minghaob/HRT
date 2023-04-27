#include "common.h"
#include "server.h"
#include <timeapi.h>

bool Server::Start()
{
	if (_is_running)
		return true;

	// start http server
	{
		_http_server.config.port = 12177;
		_http_server.config.address = "127.0.0.1";
		_http_server.config.reuse_address = false;

		//// GET-example simulating heavy work in a separate thread
		//_http_server.resource["^/data$"]["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> /*request*/) {
		//	response->write("some data");
		//};

		_http_server.resource["^/$"]["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
			std::string path = "index.html";
			std::ifstream ifs(path, std::ios::ate | std::ios::binary);
			if (!ifs.is_open())
				response->write("index.html missing from disk");

			auto length = ifs.tellg();
			ifs.seekg(0, std::ios::beg);

			SimpleWeb::CaseInsensitiveMultimap header;
			header.emplace("Content-Length", std::to_string(length));
			header.emplace("Content-Type", "text/html; charset=UTF-8");
			response->write(header);

			if (length == 0)
				response->write("");
			else
			{
				std::vector<char> buffer(length);
				ifs.read(&buffer[0], length);
				response->write(&buffer[0], length);
			}
		};

		_http_server.resource["^/img$"]["GET"] = [this](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
			std::lock_guard<std::mutex> lg(_last_image_mutex);
			if (!_last_image_jpg_valid)
			{
				if (_last_image.empty())
				{
					SimpleWeb::CaseInsensitiveMultimap header;
					header.emplace("Content-Length", "8");
					header.emplace("Content-Type", "text/html; charset=UTF-8");
					response->write(header);
					response->write("No Input");
					return;
				}

				cv::imencode(".jpg", _last_image, _last_image_jpg);
				_last_image_jpg_valid = true;
			}

			SimpleWeb::CaseInsensitiveMultimap header;
			header.emplace("Content-Length", std::to_string(_last_image_jpg.size()));
			header.emplace("Content-Type", "image/jpeg");
			response->write(header);
			response->write((char*)&_last_image_jpg[0], _last_image_jpg.size());
		};

		std::promise<unsigned short> server_port;
		_http_server_thread = std::thread([this, &server_port]() {
			try {
				// Start server
				_http_server.start([&server_port](unsigned short port) {
					server_port.set_value(port);
				});
			}
			catch (std::system_error &x) {
				if (x.code().value() == 10048)
				{
					std::cout << "Http server cannot listen on port " << _http_server.config.port << ". It's used by another program." << std::endl;
					server_port.set_value(0);
				}
			}
		});
		uint16_t port = server_port.get_future().get();
		if (port == 0)
		{
			_http_server_thread.join();
			return false;
		}
		std::cout << "Http server listening on http://localhost:" << port << std::endl;
	}

	// start websocket server
	{
		_ws_server.config.port = 12178;
		_ws_server.config.address = "127.0.0.1";
		_ws_server.config.reuse_address = false;
		auto& ep = _ws_server.endpoint["^/data$"];

		ep.on_open = [this](std::shared_ptr<WsServer::Connection> connection) {
			std::unique_lock<std::shared_mutex> lock(_ws_connection_mutex);
			_ws_connections.insert(connection);
			std::cout << "New websocket connection " << connection->remote_endpoint().address().to_string() << std::endl;
		};
		ep.on_close = [this](std::shared_ptr<WsServer::Connection> connection, int status, const std::string& /*reason*/) {
			std::unique_lock<std::shared_mutex> lock(_ws_connection_mutex);
			_ws_connections.erase(connection);
			std::cout << "Websocket Connection closed " << connection->remote_endpoint().address().to_string() << std::endl;
		};
		ep.on_message = [](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message) {
			static DWORD tbegin = ::timeGetTime();
			std::cout << "ws server: Message received: size \"" << in_message->size() << "\" from " << connection.get() << " " << ::timeGetTime() - tbegin << std::endl;
			tbegin = ::timeGetTime();
		};

		std::promise<unsigned short> server_port;
		_ws_server_thread = std::thread([this, &server_port]() {
			try {
				// Start server
				_ws_server.start([&server_port](unsigned short port) {
					server_port.set_value(port);
				});
			}
			catch (std::system_error& x) {
				if (x.code().value() == 10048)
				{
					std::cout << "Websocket server cannot listen on port " << _ws_server.config.port << ". It's used by another program." << std::endl;
					server_port.set_value(0);
				}
			}
		});
		uint16_t port = server_port.get_future().get();
		if (port == 0)
		{
			_http_server.stop();
			_http_server_thread.join();
			_ws_server_thread.join();
			return false;
		}
		std::cout << "Websocket server listening on ws://localhost:" << port << std::endl;
	}

	// start broadcast thread
	_broadcast_thread = std::thread([this]() {
		std::unique_lock<std::mutex> lock(_push_mutex);
		while (1)
		{
			_push_cv.wait(lock, [this] { return _broadcast_str.size() > 0 || !_is_running; });
			if (!_is_running)
				break;
			std::shared_lock<std::shared_mutex> lock(_ws_connection_mutex);
			for (const std::shared_ptr<WsServer::Connection>& connection : _ws_connections)
				connection->send(_broadcast_str);
			_broadcast_str.clear();
		}
	});


	_is_running = true;

	return true;
}

void Server::Stop()
{
	if (!_is_running)
		return;

	_is_running = false;

	// stop broadcast thread
	_push_cv.notify_one();
	_broadcast_thread.join();

	// stop the servers and threads
	_ws_server.stop();
	_ws_server_thread.join();
	_ws_connections.clear();
	_http_server.stop();
	_http_server_thread.join();
}

void Server::PushMessage(const std::string &msg)
{
	if (!_is_running)
		return;

	{
		std::lock_guard lg(_push_mutex);
		_broadcast_str = msg;
	}
	_push_cv.notify_one();
}

void Server::SetLastImage(cv::Mat img)
{
	std::unique_lock<std::mutex> lock(_last_image_mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return;
	_last_image = img;
	_last_image_jpg_valid = false;
}