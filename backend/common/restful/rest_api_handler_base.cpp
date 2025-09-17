#include "rest_api_handler_base.hpp"

namespace common {

http::response<http::string_body> RestApiHandlerBase::createJsonResponse(
  http::status status, const nlohmann::json& json) {
  
  http::response<http::string_body> res{status, 11};
  res.set(http::field::content_type, "application/json");
  res.body() = json.dump();
  res.prepare_payload();
  return res;
}

http::response<http::string_body> RestApiHandlerBase::createErrorResponse(
  http::status status, const std::string& message) {
  
  nlohmann::json error_json = {
    {"success", false},
    {"error", message}
  };
  return createJsonResponse(status, error_json);
}

nlohmann::json RestApiHandlerBase::parseRequestBody(const std::string& body) {
  try {
    if (body.empty()) {
        return nlohmann::json{};
    }
    return nlohmann::json::parse(body);
  } catch (const std::exception& e) {
    throw std::runtime_error("Invalid JSON in request body: " + std::string(e.what()));
  }
}

}
