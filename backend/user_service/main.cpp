#include <iostream>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <boost/asio.hpp>
#include "infrastructure/mysql_user_repository.hpp"
#include "application/auth_service.hpp"
#include "interface/user_service_impl.hpp"
#include "interface/rest_api_handler.hpp"
#include "common/config.hpp"
#include <sstream>

int main(int argc, char** argv) {
  try {
    const auto& cfg = config::Config::getInstance();
    const auto& service_config = cfg.getUserService();
    
    // 构建gRPC服务地址
    std::stringstream grpc_server_address;
    grpc_server_address << service_config.host << ":" << service_config.port;

    // 初始化存储层
    auto repository = std::make_shared<user_service::MysqlUserRepository>();

    // 初始化认证服务
    auto auth_service = std::make_shared<user_service::AuthService>(
      repository
    );

    // 初始化gRPC服务
    user_service::UserServiceImpl grpc_service(auth_service);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(grpc_server_address.str(), grpc::InsecureServerCredentials());
    builder.RegisterService(&grpc_service);
    
    std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
    std::cout << "gRPC Server listening on " << grpc_server_address.str() << std::endl;

    // 启动REST API服务器
    boost::asio::io_context ioc{1};
    
    // HTTP服务器监听在不同端口 (gRPC端口 + 1000)
    auto http_port = service_config.port + 1000;
    auto http_endpoint = boost::asio::ip::tcp::endpoint{
      boost::asio::ip::make_address(service_config.host), 
      static_cast<unsigned short>(http_port)
    };
    
    user_service::HttpServer http_server{ioc, http_endpoint, auth_service};
    
    std::cout << "HTTP Server listening on " << service_config.host << ":" << http_port << std::endl;
    
    // 在单独线程运行HTTP服务器
    std::thread http_thread([&ioc, &http_server]() {
      http_server.run();
      ioc.run();
    });

    // 主线程运行gRPC服务器
    grpc_server->Wait();
    
    // 停止HTTP服务器
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