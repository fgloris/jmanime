#include "rest_api_handler.hpp"

namespace video_service {

RestApiHandler::RestApiHandler(std::shared_ptr<VideoService> video_service)
    : video_service_(video_service) {}

http::response<http::string_body> RestApiHandler::doHandleRequest(
    http::request<http::string_body,
                  http::basic_fields<std::allocator<char>>> &&req) {
  std::string target = std::string(req.target());

  if (target == "/api/video/upload" && req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleUploadVideo(body);
  } else if (target == "/api/video/info" && req.method() == http::verb::get) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleGetVideoInfo(body);
  } else {
    return createErrorResponse(http::status::not_found, "Endpoint not found");
  }
}

http::response<http::string_body>
RestApiHandler::handleUploadVideo(const nlohmann::json &body) {
  // Placeholder implementation
  nlohmann::json response_json = {{"success", true},
                                  {"message", "Video upload started"}};
  return createJsonResponse(http::status::ok, response_json);
}

http::response<http::string_body>
RestApiHandler::handleGetVideoInfo(const nlohmann::json &body) {
  // Placeholder implementation
  nlohmann::json response_json = {{"success", true},
                                  {"video_title", "Example Video"}};
  return createJsonResponse(http::status::ok, response_json);
}

} // namespace video_service
