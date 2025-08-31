#include "video_service_impl.hpp"

namespace video_service {
VideoServiceImpl::VideoServiceImpl(std::shared_ptr<VideoService> service)
  : service_(service) {}
  
grpc::Status VideoServiceImpl::UploadVideo(grpc::ServerContext* context,
                                         const video::UploadVideoRequest* request,
                                         video::UploadVideoResponse* response) {
  
  auto result = service_->uploadVideo(
    request->auth_token(),
    request->url(),
    request->title(),
    request->description()
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  auto* video = response->mutable_video();
  video->set_uuid(result->uuid);
  
  // Set storage info
  auto* storage = video->mutable_storage();
  storage->set_path(result->storage.path);
  
  // Set format info
  auto* format_msg = storage->mutable_format();
  format_msg->set_width(result->storage.format.width);
  format_msg->set_height(result->storage.format.height);
  format_msg->set_bitrate(result->storage.format.bitrate);
  format_msg->set_format(result->storage.format.format);
  format_msg->set_codec(result->storage.format.codec);

  // Set presentation info
  auto* info = video->mutable_info();
  info->set_title(result->info.title);
  info->set_description(result->info.description);
  info->set_url(result->info.url);
  info->set_created_at(result->info.created_at);
  info->set_updated_at(result->info.updated_at);
  info->set_duration(result->info.duration);
  
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::DownloadVideo(grpc::ServerContext* context,
                                           const video::DownloadVideoRequest* request,
                                           video::DownloadVideoResponse* response) {
  auto result = service_->downloadVideo(
    request->auth_token(),
    request->video_id()
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  response->set_download_url(result.value());
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::GetVideoInfo(grpc::ServerContext* context,
                                          const video::GetVideoInfoRequest* request,
                                          video::GetVideoInfoResponse* response) {
  auto result = service_->getVideoInfo(
    request->auth_token(),
    request->video_id()
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  auto* video = response->mutable_video();
  video->set_uuid(result->uuid);
  
  // Set storage info
  auto* storage = video->mutable_storage();
  storage->set_path(result->storage.path);
  
  // Set format info
  auto* video_format = storage->mutable_format();
  video_format->set_width(result->storage.format.width);
  video_format->set_height(result->storage.format.height);
  video_format->set_bitrate(result->storage.format.bitrate);
  video_format->set_format(result->storage.format.format);
  video_format->set_codec(result->storage.format.codec);

  // Set presentation info
  auto* info = video->mutable_info();
  info->set_title(result->info.title);
  info->set_description(result->info.description);
  info->set_url(result->info.url);
  info->set_created_at(result->info.created_at);
  info->set_updated_at(result->info.updated_at);
  info->set_duration(result->info.duration);
  
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::ListVideos(grpc::ServerContext* context,
                                        const video::ListVideosRequest* request,
                                        video::ListVideosResponse* response) {
  auto result = service_->listVideos(request->auth_token());
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  for (const auto& video_file : result.value()) {
    auto* video = response->add_videos();
    video->set_uuid(video_file.uuid);

    // Set storage info
    auto* storage = video->mutable_storage();
    storage->set_path(video_file.storage.path);

    // Set format info
    auto* video_format = storage->mutable_format();
    video_format->set_width(video_file.storage.format.width);
    video_format->set_height(video_file.storage.format.height);
    video_format->set_bitrate(video_file.storage.format.bitrate);
    video_format->set_format(video_file.storage.format.format);
    video_format->set_codec(video_file.storage.format.codec);

    // Set presentation info
    auto* info = video->mutable_info();
    info->set_title(video_file.info.title);
    info->set_description(video_file.info.description);
    info->set_url(video_file.info.url);
    info->set_created_at(video_file.info.created_at);
    info->set_updated_at(video_file.info.updated_at);
    info->set_duration(video_file.info.duration);
  }
  
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::StartStreaming(grpc::ServerContext* context,
                                           const video::StartStreamingRequest* request,
                                           video::StartStreamingResponse* response) {
  auto result = service_->startStreaming(
    request->auth_token(),
    request->video_id()
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  response->set_stream_url(result.value());
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::DeleteVideo(grpc::ServerContext* context,
                                         const video::DeleteVideoRequest* request,
                                         video::DeleteVideoResponse* response) {
  auto result = service_->deleteVideo(
    request->auth_token(),
    request->video_id()
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  return grpc::Status::OK;
}
}
