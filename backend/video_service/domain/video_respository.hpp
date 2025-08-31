#pragma once

// project
#include "video.hpp"

// std
#include <vector>
#include <string>
#include <expected>

namespace video_service {

class VideoRepository {
public:
  virtual ~VideoRepository() = default;
  virtual std::expected<VideoFile, std::string> save(const VideoFile& video) = 0;
  virtual std::expected<VideoFile, std::string> findById(const std::string& id) = 0;
  virtual std::expected<std::vector<VideoFile>, std::string> findAll() = 0;
  virtual std::expected<bool, std::string> remove(const std::string& id) = 0;
};

} // namespace video_service