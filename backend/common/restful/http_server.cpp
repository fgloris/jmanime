#include "http_server.hpp"
#include <iostream>

namespace common {

// HttpServer implementation
HttpServer::HttpServer(net::io_context& ioc, tcp::endpoint endpoint, 
                       std::shared_ptr<RestApiHandlerBase> api_handler)
  : ioc_(ioc), acceptor_(ioc), api_handler_(api_handler) {
  
  beast::error_code ec;
  
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    throw std::runtime_error("Failed to open acceptor: " + ec.message());
  }
  
  acceptor_.set_option(net::socket_base::reuse_address(true), ec);
  if (ec) {
    throw std::runtime_error("Failed to set reuse_address: " + ec.message());
  }
  
  acceptor_.bind(endpoint, ec);
  if (ec) {
    throw std::runtime_error("Failed to bind: " + ec.message());
  }
  
  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    throw std::runtime_error("Failed to listen: " + ec.message());
  }
}

void HttpServer::run() {
  doAccept();
}

void HttpServer::doAccept() {
  acceptor_.async_accept(
    net::make_strand(ioc_),
    beast::bind_front_handler(&HttpServer::onAccept, this));
}

void HttpServer::onAccept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    std::cerr << "Accept error: " << ec.message() << std::endl;
  } else {
    std::make_shared<HttpSession>(std::move(socket), api_handler_)->run();
  }
  
  doAccept();
}

// HttpSession implementation
HttpSession::HttpSession(tcp::socket&& socket, std::shared_ptr<RestApiHandlerBase> api_handler)
  : stream_(std::move(socket)), api_handler_(api_handler) {}

void HttpSession::run() {
  net::dispatch(stream_.get_executor(),
                beast::bind_front_handler(&HttpSession::doRead, shared_from_this()));
}

void HttpSession::doRead() {
  req_ = {};
  
  stream_.expires_after(std::chrono::seconds(30));
  
  http::async_read(stream_, buffer_, req_,
                   beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
}

void HttpSession::onRead(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);
  
  if (ec == http::error::end_of_stream) {
    return doClose();
  }
  
  if (ec) {
    std::cerr << "Read error: " << ec.message() << std::endl;
    return;
  }
  
  auto response = std::make_shared<http::response<http::string_body>>(
    api_handler_->handleRequest(std::move(req_)));
  
  res_ = response;
  
  http::async_write(stream_, *response,
                    beast::bind_front_handler(&HttpSession::onWrite, shared_from_this(),
                                            response->need_eof()));
}

void HttpSession::onWrite(bool close, beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);
  
  if (ec) {
    std::cerr << "Write error: " << ec.message() << std::endl;
    return;
  }
  
  if (close) {
    return doClose();
  }
  
  res_ = nullptr;
  doRead();
}

void HttpSession::doClose() {
  beast::error_code ec;
  stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

}
