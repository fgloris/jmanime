#include "video_service.hpp"
#include "common/config/config.hpp"
#include "domain/video.hpp"
#include "user.grpc.pb.h"
#include <uuid/uuid.h>
#include <grpcpp/grpcpp.h>

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
    storage_path_(storage_path) {
  auto& cfg = config::Config::getInstance();
  auto channel = grpc::CreateChannel(cfg.getUserServiceIpPort(), grpc::InsecureChannelCredentials());
  stub_ = user_service::UserService::NewStub(channel);
}

std::expected<VideoFile, std::string> VideoService::uploadVideo(
  const std::string& auth_token,
  const std::string& url,
  const std::string& title,
  const std::string& description
) {
  uuid_t uuid;
  uuid_generate(uuid);
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);
  
  auto download_path = storage_path_ + "/" + uuid_str + RAW_DOWNLOAD_FILE_SUFFIX;
  auto result = download_service_->download(url, download_path, auth_token);
    
  if (!result) {
    return std::unexpected(result.error());
  }

  const auto& format_cfg = config::Config::getInstance().getFormat();
  
  auto output_base_path = storage_path_ + "/" + uuid_str;
  auto transfuture = transcoding_service_->getTranscodeFuture(download_path, output_base_path, VideoFormat{});
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

std::expected<std::string, std::string> VideoService::downloadVideo(
  const std::string& auth_token,
  const std::string& video_id
) {
  auto video = repository_->findById(video_id);
  if (!video) {
    return std::unexpected("Video not found");
  }
  
  return std::expected<std::string, std::string>("/download/" + video_id + "/" + std::filesystem::path(video->storage.path).filename().string());
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

std::expected<VideoFile, std::string> VideoService::importLocalVideo(
  const std::filesystem::path& source_path,
  const std::string& auth_token
) {
  // 检查文件是否存在
  if (!std::filesystem::exists(source_path)) {
    return std::unexpected("Source file does not exist: " + source_path.string());
  }

  // 生成新的UUID
  uuid_t uuid;
  uuid_generate(uuid);
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);

  // 构建目标路径
  auto extension = source_path.extension();
  auto target_path = std::filesystem::path(storage_path_) / 
                    std::filesystem::path(std::string(uuid_str) + RAW_DOWNLOAD_FILE_SUFFIX + extension.string());

  try {
    // 复制文件到存储目录
    std::filesystem::copy_file(source_path, target_path, 
                             std::filesystem::copy_options::overwrite_existing);
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Failed to copy file: " + std::string(e.what()));
  }

  // 获取基本文件信息
  auto file_size = std::filesystem::file_size(source_path);
  
  // 使用原始文件名作为标题
  std::string title = source_path.stem().string();

  const auto& format_cfg = config::Config::getInstance().getFormat();
  
  // 转码文件
  auto output_base_path = storage_path_ + "/" + uuid_str;
  auto transfuture = transcoding_service_->getTranscodeFuture(target_path.string(), output_base_path, VideoFormat{});
  auto transresult = transfuture.get();
  if (!transresult) {
    // 清理中间文件
    std::filesystem::remove(target_path);
    return std::unexpected(transresult.error());
  }

  VideoFile video;
  video.uuid = uuid_str;
  video.storage = transresult.value();
  video.info.title = title;
  video.info.description = "Imported from local file: " + source_path.filename().string();
  video.info.url = "local://" + source_path.filename().string();
  video.info.created_at = "";//std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
  video.info.updated_at = video.info.created_at;

  // 保存到数据库
  return repository_->save(video);
}

std::expected<std::vector<VideoFile>, std::string> VideoService::importLocalVideos(
  const std::string& source_dir,
  const std::string& auth_token
) {
  std::vector<VideoFile> imported_videos;
  std::vector<std::string> errors;

  try {
    // 检查源目录是否存在
    if (!std::filesystem::exists(source_dir)) {
      return std::unexpected("Source directory does not exist: " + source_dir);
    }

    // 遍历目录中的所有文件
    for (const auto& entry : std::filesystem::directory_iterator(source_dir)) {
      // 只处理视频文件
      auto extension = entry.path().extension();
      if (extension == ".mp4" || extension == ".mkv" || extension == ".avi" || 
          extension == ".mov" || extension == ".wmv" || extension == ".flv") {
        
        auto result = importLocalVideo(entry.path(), auth_token);
        if (result) {
          imported_videos.push_back(result.value());
        } else {
          errors.push_back("Failed to import " + entry.path().string() + 
                          ": " + result.error());
        }
      }
    }

    if (!errors.empty()) {
      std::string error_message = "Some files failed to import:\n";
      for (const auto& err : errors) {
        error_message += err + "\n";
      }
      return std::unexpected(error_message);
    }

    return imported_videos;

  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected("Filesystem error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Unexpected error: " + std::string(e.what()));
  }
}

}