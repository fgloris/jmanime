#include <iostream>
#include <grpcpp/grpcpp.h>
#include "common/config.hpp"
#include "interface/video_service_impl.hpp"
#include "application/video_service.hpp"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace video_service;

int main(int argc, char** argv) {
  // 创建客户端
  const auto& cfg = config::Config::getInstance();

  auto channel = grpc::CreateChannel(cfg.getVideoServiceIpPort(), grpc::InsecureChannelCredentials());
  auto stub = video::VideoService::NewStub(channel);

  // 创建请求
  video::ImportLocalVideosRequest request;
  request.set_auth_token(cfg.getAuth().jwt_secret);
  request.set_source_dir("/home/ginger/Videos/《瑞克和莫蒂》");

  // 发送请求
  video::ImportLocalVideosResponse response;
  grpc::ClientContext context;
  Status status = stub->ImportLocalVideos(&context, request, &response);

  if (status.ok()) {
      if (response.success()) {
          std::cout << "Successfully imported " << response.videos_size() << " videos\n";
          for (const auto& video : response.videos()) {
              std::cout << "- " << video.info().title() << "\n";
          }
      } else {
          std::cout << "Import failed: " << response.message() << "\n";
      }
  } else {
      std::cout << "RPC failed: " << status.error_message() << "\n";
  }

  return 0;
}
