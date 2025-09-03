#pragma once
#include "application/video_service.hpp"
#include "video.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace video_service {
class VideoServiceImpl final : public video::VideoService::Service {
public:
  VideoServiceImpl(std::shared_ptr<VideoService> service);
  
  grpc::Status UploadVideo(grpc::ServerContext* context,
                          const video::UploadVideoRequest* request,
                          video::UploadVideoResponse* response) override;
                          
  grpc::Status DownloadVideo(grpc::ServerContext* context,
                            const video::DownloadVideoRequest* request,
                            video::DownloadVideoResponse* response) override;
                            
  grpc::Status GetVideoInfo(grpc::ServerContext* context,
                           const video::GetVideoInfoRequest* request,
                           video::GetVideoInfoResponse* response) override;
                           
  grpc::Status ListVideos(grpc::ServerContext* context,
                         const video::ListVideosRequest* request,
                         video::ListVideosResponse* response) override;

  grpc::Status StartStreaming(grpc::ServerContext* context,
                             const video::StartStreamingRequest* request,
                             video::StartStreamingResponse* response) override;
                         
  grpc::Status DeleteVideo(grpc::ServerContext* context,
                          const video::DeleteVideoRequest* request,
                          video::DeleteVideoResponse* response) override;

  grpc::Status ImportLocalVideos(grpc::ServerContext* context,
                             const video::ImportLocalVideosRequest* request,
                             video::ImportLocalVideosResponse* response) override;
private:
  std::shared_ptr<VideoService> service_;
};
}