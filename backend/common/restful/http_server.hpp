#pragma once
#include <memory>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include "common/restful/rest_api_handler_base.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace common {

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  HttpSession(tcp::socket&& socket, std::shared_ptr<RestApiHandlerBase> api_handler);
  
  void run();

private:
  void doRead();
  void onRead(beast::error_code ec, std::size_t bytes_transferred);
  void onWrite(bool close, beast::error_code ec, std::size_t bytes_transferred);
  void doClose();

  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  std::shared_ptr<void> res_;
  std::shared_ptr<RestApiHandlerBase> api_handler_;
};

class HttpServer {
public:
  HttpServer(net::io_context& ioc, tcp::endpoint endpoint, 
             std::shared_ptr<RestApiHandlerBase> api_handler);
  
  void run();

private:
  void doAccept();
  void onAccept(beast::error_code ec, tcp::socket socket);

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::shared_ptr<RestApiHandlerBase> api_handler_;
};

}
