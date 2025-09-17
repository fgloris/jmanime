#pragma once
#include "application/video_service.hpp"
#include "common/restful/rest_api_handler_base.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace video_service {

class RestApiHandler : public common::RestApiHandlerBase {
public:
  RestApiHandler(std::shared_ptr<VideoService> video_service);

protected:
  http::response<http::string_body> doHandleRequest(
      http::request<http::string_body,
                    http::basic_fields<std::allocator<char>>> &&req) override;

private:
  std::shared_ptr<VideoService> video_service_;

  http::response<http::string_body> handleUploadVideo(const nlohmann::json &body);
  http::response<http::string_body> handleGetVideoInfo(const nlohmann::json &body);
};

} // namespace video_service
