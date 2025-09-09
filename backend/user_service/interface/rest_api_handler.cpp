#include "rest_api_handler.hpp"
#include <iostream>

namespace user_service {

RestApiHandler::RestApiHandler(std::shared_ptr<AuthService> auth_service)
  : auth_service_(auth_service) {}

template<class Body, class Allocator>
http::response<http::string_body> RestApiHandler::handleRequest(
  http::request<Body, http::basic_fields<Allocator>>&& req) {
  
  // CORS headers
  auto addCorsHeaders = [](auto& res) {
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
  };

  // Handle OPTIONS request for CORS
  if (req.method() == http::verb::options) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    addCorsHeaders(res);
    res.prepare_payload();
    return res;
  }

  std::string target = std::string(req.target());
  
  try {
    if (target == "/api/auth/validate-email" && req.method() == http::verb::post) {
      auto body = parseRequestBody(std::string(req.body()));
      auto response = handleValidateEmail(body);
      addCorsHeaders(response);
      return response;
    }
    else if (target == "/api/auth/register" && req.method() == http::verb::post) {
      auto body = parseRequestBody(std::string(req.body()));
      auto response = handleRegister(body);
      addCorsHeaders(response);
      return response;
    }
    else if (target == "/api/auth/login" && req.method() == http::verb::post) {
      auto body = parseRequestBody(std::string(req.body()));
      auto response = handleLogin(body);
      addCorsHeaders(response);
      return response;
    }
    else if (target == "/api/auth/validate-token" && req.method() == http::verb::post) {
      auto auth_header = req[http::field::authorization];
      if (auth_header.empty()) {
        auto response = createErrorResponse(http::status::unauthorized, "Missing authorization header");
        addCorsHeaders(response);
        return response;
      }
      
      std::string token = std::string(auth_header);
      if (token.starts_with("Bearer ")) {
        token = token.substr(7);
      }
      
      auto response = handleValidateToken(token);
      addCorsHeaders(response);
      return response;
    }
    else {
      auto response = createErrorResponse(http::status::not_found, "Endpoint not found");
      addCorsHeaders(response);
      return response;
    }
  } catch (const std::exception& e) {
    auto response = createErrorResponse(http::status::internal_server_error, 
                                      "Internal server error: " + std::string(e.what()));
    addCorsHeaders(response);
    return response;
  }
}

http::response<http::string_body> RestApiHandler::handleValidateEmail(const nlohmann::json& body) {
  try {
    if (!body.contains("email")) {
      return createErrorResponse(http::status::bad_request, "Missing email field");
    }
    
    std::string email = body["email"];
    auto result = auth_service_->registerSendEmailVerificationCode(email);
    
    if (result) {
      nlohmann::json response_json = {
        {"success", true},
        {"message", "Verification code sent successfully"},
        {"code", result.value()} // 开发时返回验证码，生产环境应移除
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::bad_request, result.error());
    }
  } catch (const std::exception& e) {
    return createErrorResponse(http::status::internal_server_error, 
                              "Failed to process email validation: " + std::string(e.what()));
  }
}

http::response<http::string_body> RestApiHandler::handleRegister(const nlohmann::json& body) {
  try {
    // 验证必要字段
    std::vector<std::string> required_fields = {"email", "verification_code", "username", "password"};
    for (const auto& field : required_fields) {
      if (!body.contains(field)) {
        return createErrorResponse(http::status::bad_request, "Missing field: " + field);
      }
    }
    
    std::string email = body["email"];
    std::string verification_code = body["verification_code"];
    std::string username = body["username"];
    std::string password = body["password"];
    std::string avatar = body.value("avatar", "");
    
    auto result = auth_service_->registerAndStore(email, verification_code, username, password, avatar);
    
    if (result) {
      auto [token, user] = result.value();
      nlohmann::json response_json = {
        {"success", true},
        {"message", "User registered successfully"},
        {"token", token},
        {"user", {
          {"id", user.id()},
          {"email", user.email()},
          {"username", user.username()},
          {"avatar", user.avatar()}
        }}
      };
      return createJsonResponse(http::status::created, response_json);
    } else {
      return createErrorResponse(http::status::bad_request, result.error());
    }
  } catch (const std::exception& e) {
    return createErrorResponse(http::status::internal_server_error, 
                              "Failed to process registration: " + std::string(e.what()));
  }
}

http::response<http::string_body> RestApiHandler::handleLogin(const nlohmann::json& body) {
  try {
    if (!body.contains("email") || !body.contains("password")) {
      return createErrorResponse(http::status::bad_request, "Missing email or password");
    }
    
    std::string email = body["email"];
    std::string password = body["password"];
    
    auto result = auth_service_->login(email, password);
    
    if (result) {
      auto [token, user] = result.value();
      nlohmann::json response_json = {
        {"success", true},
        {"message", "Login successful"},
        {"token", token},
        {"user", {
          {"id", user.id()},
          {"email", user.email()},
          {"username", user.username()},
          {"avatar", user.avatar()}
        }}
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::unauthorized, result.error());
    }
  } catch (const std::exception& e) {
    return createErrorResponse(http::status::internal_server_error, 
                              "Failed to process login: " + std::string(e.what()));
  }
}

http::response<http::string_body> RestApiHandler::handleValidateToken(const std::string& token) {
  try {
    auto result = auth_service_->validateToken(token);
    
    if (result) {
      nlohmann::json response_json = {
        {"success", true},
        {"message", "Token is valid"},
        {"user_id", result.value()}
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::unauthorized, result.error());
    }
  } catch (const std::exception& e) {
    return createErrorResponse(http::status::internal_server_error, 
                              "Failed to validate token: " + std::string(e.what()));
  }
}

http::response<http::string_body> RestApiHandler::createJsonResponse(
  http::status status, const nlohmann::json& json) {
  
  http::response<http::string_body> res{status, 11};
  res.set(http::field::content_type, "application/json");
  res.body() = json.dump();
  res.prepare_payload();
  return res;
}

http::response<http::string_body> RestApiHandler::createErrorResponse(
  http::status status, const std::string& message) {
  
  nlohmann::json error_json = {
    {"success", false},
    {"error", message}
  };
  return createJsonResponse(status, error_json);
}

nlohmann::json RestApiHandler::parseRequestBody(const std::string& body) {
  try {
    return nlohmann::json::parse(body);
  } catch (const std::exception& e) {
    throw std::runtime_error("Invalid JSON in request body: " + std::string(e.what()));
  }
}

// HttpServer implementation
HttpServer::HttpServer(net::io_context& ioc, tcp::endpoint endpoint, 
                       std::shared_ptr<AuthService> auth_service)
  : ioc_(ioc), acceptor_(ioc), api_handler_(std::make_shared<RestApiHandler>(auth_service)) {
  
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
HttpSession::HttpSession(tcp::socket&& socket, std::shared_ptr<RestApiHandler> api_handler)
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
  
  // 处理请求并生成响应
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
  if (ec) {
    std::cerr << "Shutdown error: " << ec.message() << std::endl;
  }
}

// 显式实例化模板
template http::response<http::string_body> RestApiHandler::handleRequest<http::string_body, std::allocator<char>>(
  http::request<http::string_body, http::basic_fields<std::allocator<char>>>&&);

}