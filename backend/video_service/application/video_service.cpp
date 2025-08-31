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
  
  auto download_path = storage_path_ + "/" + uuid_str + "_original";
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
  
  auto output_path = storage_path_ + "/" + uuid_str;
  auto transcoded = transcoding_service_->getTranscodeFuture(download_path, output_path, format).get();
  if (!transcoded) {
    return std::unexpected(transcoded.error());
  }
  
  VideoFile video;
  video.id = uuid_str;
  video.path = output_path;
  video.format = format;
  video.metadata.title = title;
  video.metadata.description = description;
  video.metadata.url = url;
  
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
  
  std::filesystem::remove(video->path);
  return repository_->remove(video_id);
}
}