#pragma once
#include <memory>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;

namespace common {

class RestApiHandlerBase {
public:
  virtual ~RestApiHandlerBase() = default;

  template<class Body, class Allocator>
  http::response<http::string_body> handleRequest(
    http::request<Body, http::basic_fields<Allocator>>&& req) {
    
    auto addCorsHeaders = [](auto& res) {
      res.set(http::field::access_control_allow_origin, "*");
      res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
      res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
    };

    if (req.method() == http::verb::options) {
      http::response<http::string_body> res{http::status::ok, req.version()};
      addCorsHeaders(res);
      res.prepare_payload();
      return res;
    }

    try {
      auto response = doHandleRequest(std::move(req));
      addCorsHeaders(response);
      return response;
    } catch (const std::exception& e) {
      auto response = createErrorResponse(http::status::internal_server_error, 
                                        "Internal server error: " + std::string(e.what()));
      addCorsHeaders(response);
      return response;
    }
  }

protected:
  virtual http::response<http::string_body> doHandleRequest(
    http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req) = 0;

  http::response<http::string_body> createJsonResponse(
    http::status status, const nlohmann::json& json);
  
  http::response<http::string_body> createErrorResponse(
    http::status status, const std::string& message);
  
  nlohmann::json parseRequestBody(const std::string& body);
};

}
