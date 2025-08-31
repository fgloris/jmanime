#include "video_service_impl.hpp"

namespace video_service {
VideoServiceImpl::VideoServiceImpl(std::shared_ptr<VideoService> service)
  : service_(service) {}
  
grpc::Status VideoServiceImpl::UploadVideo(grpc::ServerContext* context,
                                         const video::UploadVideoRequest* request,
                                         video::UploadVideoResponse* response) {
  VideoFormat format{
    .width = request->format().width(),
    .height = request->format().height(),
    .bitrate = request->format().bitrate(),
    .format = request->format().format(),
    .codec = request->format().codec()
  };
  
  auto result = service_->uploadVideo(
    request->auth_token(),
    request->url(),
    request->title(),
    request->description(),
    format
  );
  
  if (!result) {
    response->set_success(false);
    response->set_message(result.error());
    return grpc::Status::OK;
  }
  
  response->set_success(true);
  auto* video = response->mutable_video();
  video->set_id(result->id);
  video->set_path(result->path);
  
  auto* video_format = video->mutable_format();
  video_format->set_width(result->format.width);
  video_format->set_height(result->format.height);
  video_format->set_bitrate(result->format.bitrate);
  video_format->set_format(result->format.format);
  video_format->set_codec(result->format.codec);
  
  return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::DownloadVideo(grpc::ServerContext* context,
                                           const video::DownloadVideoRequest* request,
                                           video::DownloadVideoResponse* response) {
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
  video->set_id(result->id);
  video->set_path(result->path);
  
  auto* format = video->mutable_format();
  format->set_width(result->format.width);
  format->set_height(result->format.height);
  format->set_bitrate(result->format.bitrate);
  format->set_format(result->format.format);
  format->set_codec(result->format.codec);
  
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
    video->set_id(video_file.id);
    video->set_path(video_file.path);
    
    auto* format = video->mutable_format();
    format->set_width(video_file.format.width);
    format->set_height(video_file.format.height);
    format->set_bitrate(video_file.format.bitrate);
    format->set_format(video_file.format.format);
    format->set_codec(video_file.format.codec);
  }
  
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
