#include "interface/video_service_impl.hpp"
#include "infrastructure/mysql_video_repository.hpp"
#include "infrastructure/downloader.hpp"
#include "infrastructure/hls_server.hpp"
#include "infrastructure/mp4_transcoder.hpp"
#include "common/config.hpp"
#include <grpcpp/server_builder.h>
#include <filesystem>
#include <sstream>
#include <thread>

int main(int argc, char** argv) {
  try {
    const auto& cfg = config::Config::getInstance();
    const auto& storage_path = cfg.getStoragePath();
    std::filesystem::create_directories(storage_path);
    
    const auto& db_config = cfg.getDatabase();
    std::shared_ptr<video_service::VideoRepository> repository = 
      std::make_shared<video_service::MysqlVideoRepository>(
        db_config.host,
        db_config.user,
        db_config.password,
        db_config.db_name
      );
  
  std::shared_ptr<video_service::DownloadService> download_service = 
    std::make_shared<video_service::Downloader>();
  
  const auto& streaming_config = cfg.getStreaming();
  std::shared_ptr<video_service::StreamingService> streaming_service = 
    std::make_shared<video_service::HlsServer>(
      streaming_config.host,
      streaming_config.port
    );
  
  std::shared_ptr<video_service::TranscodingService> transcoding_service = 
    std::make_shared<video_service::MP4Transcoder>();
  
  auto video_service = std::make_shared<video_service::VideoService>(
    repository, download_service, streaming_service, transcoding_service, storage_path
  );
  
  video_service::VideoServiceImpl service(video_service);
  
  const auto& service_config = cfg.getVideoService();
  std::stringstream server_address;
  server_address << service_config.host << ":" << service_config.port;
  
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address.str(), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address.str() << std::endl;
  
  // Start HLS server in background thread
  std::thread hls_thread([streaming_service, storage_path]() {
    streaming_service->startServer(storage_path);
  });
  
  server->Wait();
  
  // Stop HLS server
  streaming_service->stopServer();
  hls_thread.join();
  
  return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}