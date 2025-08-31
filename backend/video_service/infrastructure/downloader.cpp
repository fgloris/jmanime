#include "downloader.hpp"
#include <fstream>

namespace video_service {
Downloader::Downloader() : should_stop_(false) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl_ = curl_easy_init();
  if (!curl_) {
    throw std::runtime_error("Failed to initialize CURL");
  }
}

Downloader::~Downloader() {
  curl_easy_cleanup(curl_);
  curl_global_cleanup();
}

std::expected<std::string, std::string> Downloader::download(
  const std::string& url,
  const std::string& output_path,
  const std::string& auth_token,
  ProgressCallback progress_callback
) {
  should_stop_ = false;
  progress_callback_ = progress_callback;
  
  std::ofstream file(output_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open output file");
  }
  
  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &file);
  curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl_, CURLOPT_PROGRESSFUNCTION, progressCallback);
  curl_easy_setopt(curl_, CURLOPT_PROGRESSDATA, this);
  curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
  
  struct curl_slist* headers = nullptr;
  if (!auth_token.empty()) {
    headers = curl_slist_append(headers, 
      ("Authorization: Bearer " + auth_token).c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
  }
  
  auto res = curl_easy_perform(curl_);
  if (headers) {
    curl_slist_free_all(headers);
  }
  
  if (res != CURLE_OK) {
    return std::unexpected(curl_easy_strerror(res));
  }
  
  long http_code = 0;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code != 200) {
    return std::unexpected("HTTP error: " + std::to_string(http_code));
  }
  
  return output_path;
}

void Downloader::stopDownload() {
  
}

size_t Downloader::writeCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* file = static_cast<std::ofstream*>(userdata);
  size_t written = file->write(static_cast<char*>(ptr), size * nmemb).tellp();
  return written;
}

int Downloader::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow) {
  auto* downloader = static_cast<Downloader*>(clientp);
  if (downloader->should_stop_) {
    return 1;
  }
  
  if (dltotal > 0 && downloader->progress_callback_) {
    float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
    downloader->progress_callback_(progress);
  }
  
  return 0;
}
}
