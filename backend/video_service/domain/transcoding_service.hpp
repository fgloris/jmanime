#pragma once
#include "video.hpp"
#include <expected>
#include <future>

namespace video_service {
class TranscodingService {
public:
  virtual ~TranscodingService() = default;
  virtual std::expected<VideoFileStorage, std::string> transcode(
    const std::string& input_path,
    const std::string& output_base_path
  ) = 0; 
  virtual std::future<std::expected<VideoFileStorage, std::string>> getTranscodeFuture(
    const std::string& input,
    const std::string& output_base_path,
    const VideoFormat& format
  ) = 0;
  //virtual void waitForAll() = 0;
};
}