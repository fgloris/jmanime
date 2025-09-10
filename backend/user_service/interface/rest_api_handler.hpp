#pragma once
#include <memory>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "application/auth_service.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace user_service {

class RestApiHandler {
public:
  RestApiHandler(std::shared_ptr<AuthService> auth_service);
  
  template<class Body, class Allocator>
  http::response<http::string_body> handleRequest(
    http::request<Body, http::basic_fields<Allocator>>&& req);

private:
  std::shared_ptr<AuthService> auth_service_;
  
  http::response<http::string_body> handleRegisterValidateEmail(const nlohmann::json& body);
  http::response<http::string_body> handleRegister(const nlohmann::json& body);
  http::response<http::string_body> handleLoginEmailPwd(const nlohmann::json& body);
  http::response<http::string_body> handleLoginEmailCode(const nlohmann::json& body);
  http::response<http::string_body> handleLoginValidateEmail(const nlohmann::json& body);
  http::response<http::string_body> handleValidateToken(const std::string& token);
  
  http::response<http::string_body> createJsonResponse(
    http::status status, const nlohmann::json& json);
  http::response<http::string_body> createErrorResponse(
    http::status status, const std::string& message);
  
  nlohmann::json parseRequestBody(const std::string& body);
};

class HttpServer {
public:
  HttpServer(net::io_context& ioc, tcp::endpoint endpoint, 
             std::shared_ptr<AuthService> auth_service);
  
  void run();

private:
  void doAccept();
  void onAccept(beast::error_code ec, tcp::socket socket);

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::shared_ptr<RestApiHandler> api_handler_;
};

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  HttpSession(tcp::socket&& socket, std::shared_ptr<RestApiHandler> api_handler);
  
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
  std::shared_ptr<RestApiHandler> api_handler_;
};

}