#pragma once

#include <memory>
#include <vector>
#include <expected>
#include <filesystem>
#include "domain/video_respository.hpp"
#include "domain/download_service.hpp"
#include "domain/transcoding_service.hpp"
#include "domain/streaming_service.hpp"


namespace video_service {
class VideoService {
public:
  VideoService(std::shared_ptr<VideoRepository> repository,
               std::shared_ptr<DownloadService> download_service,
               std::shared_ptr<StreamingService> streaming_service,
               std::shared_ptr<TranscodingService> transcoding_service,
               const std::string& storage_path);
               
  std::expected<VideoFile, std::string> uploadVideo(
    const std::string& auth_token,
    const std::string& url,
    const std::string& title,
    const std::string& description
  );
  
  std::expected<std::string, std::string> startStreaming(
    const std::string& auth_token,
    const std::string& video_id
  );
  
  std::expected<std::string, std::string> downloadVideo(
    const std::string& auth_token,
    const std::string& video_id
  );

  std::expected<VideoFile, std::string> getVideoInfo(
    const std::string& auth_token,
    const std::string& video_id
  );
  
  std::expected<std::vector<VideoFile>, std::string> listVideos(
    const std::string& auth_token
  );
  
  std::expected<bool, std::string> deleteVideo(
    const std::string& auth_token,
    const std::string& video_id
  );

  // 导入本地文件夹中的视频
  std::expected<std::vector<VideoFile>, std::string> importLocalVideos(
    const std::string& source_dir,
    const std::string& auth_token
  );

private:
  // 导入单个视频文件
  std::expected<VideoFile, std::string> importLocalVideo(
    const std::filesystem::path& source_path,
    const std::string& auth_token
  );
  std::shared_ptr<VideoRepository> repository_;
  std::shared_ptr<DownloadService> download_service_;
  std::shared_ptr<StreamingService> streaming_service_;
  std::shared_ptr<TranscodingService> transcoding_service_;
  std::string storage_path_;
};
}