#include "video_service.hpp"
#include <uuid.h>
#include <filesystem>

namespace video_service {
VideoService::VideoService(std::shared_ptr<VideoRepository> repository,
                         std::shared_ptr<DownloadService> download_service,
                         std::shared_ptr<StreamingService> streaming_service,
                         std::shared_ptr<TranscodingService> transcoding_service,
                         const std::string& storage_path)
  : repository_(repository),
    download_service_(download_service),
    streaming_service_(streaming_service),
    transcoding_service_(transcoding_service),
    storage_path_(storage_path) {}

std::expected<VideoFile, std::string> VideoService::uploadVideo(
  const std::string& auth_token,
  const std::string& url,
  const std::string& title,
  const std::string& description,
  const VideoFormat& format
) {
  uuid_t uuid;
  uuid_generate(uuid);
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);
  
  auto download_path = storage_path_ + "/" + uuid_str + RAW_DOWNLOAD_FILE_SUFFIX;
  auto result = download_service_->download(url, download_path, auth_token,
    [this, uuid_str](float progress) {
      auto video = repository_->findById(uuid_str);
      if (video) {
        repository_->save(video.value());
      }
    });
    
  if (!result) {
    return std::unexpected(result.error());
  }
  
  static VideoFormat target_format{
    .format = "mp4",
    .codec = "libx265",
  };
  
  auto output_base_path = storage_path_ + "/" + uuid_str;
  auto transfuture = transcoding_service_->getTranscodeFuture(download_path, output_base_path, target_format);
  auto transresult = transfuture.get();
  if (!transresult) {
    return std::unexpected(transresult.error());
  }
  
  VideoFile video;
  video.uuid = uuid_str;
  video.storage = transresult.value();
  video.info.title = title;
  video.info.description = description;
  video.info.url = url;
  video.info.created_at = "";//std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
  video.info.updated_at = video.info.created_at;
  
  return repository_->save(video);
}

std::expected<std::string, std::string> VideoService::startStreaming(
  const std::string& auth_token,
  const std::string& video_id
) {
  auto video = repository_->findById(video_id);
  if (!video) {
    return std::unexpected("Video not found");
  }
  
  return streaming_service_->getStreamUrl(video_id);
}

std::expected<VideoFile, std::string> VideoService::getVideoInfo(
  const std::string& auth_token,
  const std::string& video_id
) {
  return repository_->findById(video_id);
}

std::expected<std::vector<VideoFile>, std::string> VideoService::listVideos(
  const std::string& auth_token
) {
  return repository_->findAll();
}

std::expected<bool, std::string> VideoService::deleteVideo(
  const std::string& auth_token,
  const std::string& video_id
) {
  auto video = repository_->findById(video_id);
  if (!video) {
    return std::unexpected("Video not found");
  }
  
  std::filesystem::remove(video->storage.path);
  return repository_->remove(video_id);
}
}