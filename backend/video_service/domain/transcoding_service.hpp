#pragma once
#include "video.hpp"
#include <expected>
#include <future>

namespace video_service {
class TranscodingService {
public:
  virtual ~TranscodingService() = default;
  virtual std::future<std::expected<VideoFileStorage, std::string>> getTranscodeFuture(
    const std::string& input,
    const std::string& output_base_path,
    const VideoFormat& format
  ) = 0;
};
}