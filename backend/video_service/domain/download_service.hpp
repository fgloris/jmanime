#pragma once
#include <string>
#include <expected>
#include <functional>

namespace video_service {
class DownloadService {
public:
  using ProgressCallback = std::function<void(float)>;
  virtual ~DownloadService() = default;
  virtual std::expected<std::string, std::string> download(
    const std::string& url,
    const std::string& output_path,
    const std::string& auth_token,
    ProgressCallback progress_callback = nullptr
  ) = 0;
  virtual void stopDownload() = 0;
};
}