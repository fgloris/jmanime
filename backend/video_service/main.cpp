#include "interface/video_service_impl.hpp"
#include "infrastructure/mysql_video_repository.hpp"
#include "infrastructure/downloader.hpp"
#include "infrastructure/hls_server.hpp"
#include "infrastructure/transcoder.hpp"
#include <grpcpp/server_builder.h>
#include <filesystem>
#include <thread>

int main(int argc, char** argv) {
  const std::string storage_path = "/tmp/videos";
  std::filesystem::create_directories(storage_path);
  
  std::shared_ptr<video_service::VideoRepository> repository = 
    std::make_shared<video_service::MysqlVideoRepository>(
      "localhost", "user", "password", "video_db"
    );
  
  std::shared_ptr<video_service::DownloadService> download_service = 
    std::make_shared<video_service::Downloader>();
  
  std::shared_ptr<video_service::StreamingService> streaming_service = 
    std::make_shared<video_service::HlsServer>("0.0.0.0", 8080);
  
  std::shared_ptr<video_service::TranscodingService> transcoding_service = 
    std::make_shared<video_service::Transcoder>();
  
  streaming_service->startServer(storage_path);
  
  auto video_service = std::make_shared<video_service::VideoService>(
    repository, download_service, streaming_service, transcoding_service, storage_path
  );
  
  video_service::VideoServiceImpl service(video_service);
  
  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on 0.0.0.0:50051" << std::endl;
  
  std::thread hls_thread([&hls_server]() {
    hls_server->run();
  });
  
  server->Wait();
  
  hls_server->stop();
  hls_thread.join();
  
  return 0;
}