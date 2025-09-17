#include "interface/video_service_impl.hpp"
#include "infrastructure/mysql_video_repository.hpp"
#include "infrastructure/downloader.hpp"
#include "infrastructure/hls_server.hpp"
#include "infrastructure/mp4_transcoder.hpp"
#include "common/config/config.hpp"
#include "common/restful/http_server.hpp"
#include "interface/rest_api_handler.hpp"
#include <grpcpp/server_builder.h>
#include <filesystem>
#include <sstream>
#include <thread>
#include <boost/asio.hpp>

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
  
  video_service::VideoServiceImpl grpc_service(video_service);
  
  const auto& service_config = cfg.getVideoService();
  std::stringstream grpc_server_address;
  grpc_server_address << service_config.host << ":" << service_config.port;
  
  grpc::ServerBuilder builder;
  builder.AddListeningPort(grpc_server_address.str(), grpc::InsecureServerCredentials());
  builder.RegisterService(&grpc_service);
  
  std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
  std::cout << "gRPC Server listening on " << grpc_server_address.str() << std::endl;

  // Start REST API server
  boost::asio::io_context ioc{1};
  auto http_port = service_config.port + 1000;
  auto http_endpoint = boost::asio::ip::tcp::endpoint{
    boost::asio::ip::make_address(service_config.host), 
    static_cast<unsigned short>(http_port)
  };

  auto api_handler = std::make_shared<video_service::RestApiHandler>(video_service);
  common::HttpServer http_server{ioc, http_endpoint, api_handler};

  std::cout << "HTTP Server listening on " << service_config.host << ":" << http_port << std::endl;

  std::thread http_thread([&ioc, &http_server]() {
    grpc_server->Wait();
  });
  
  // Start HLS server in background thread
  std::thread hls_thread([streaming_service, storage_path]() {
    streaming_service->startServer(storage_path);
  });
  
  http_server.run();
  ioc.run();
  
  // Stop HLS server
  streaming_service->stopServer();
  hls_thread.join();

  // Stop HTTP server
  ioc.stop();
  if (http_thread.joinable()) {
    http_thread.join();
  }
  
  return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
