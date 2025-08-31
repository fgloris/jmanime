#pragma once
#include <string>
#include <expected>
#include <functional>
#include <curl/curl.h>
#include "domain/download_service.hpp"

namespace video_service {
class Downloader : public DownloadService {
public:
  Downloader();
  ~Downloader() override;
  
  std::expected<std::string, std::string> download(
    const std::string& url,
    const std::string& output_path,
    const std::string& auth_token,
    ProgressCallback progress_callback = nullptr
  ) override;
  
  void stopDownload() override;
  
private:
  static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
  static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow);
  
  CURL* curl_;
  bool should_stop_;
  ProgressCallback progress_callback_;
};
}