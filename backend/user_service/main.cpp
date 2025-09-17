#include <iostream>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <boost/asio.hpp>
#include "infrastructure/email_queue.hpp"
#include "infrastructure/mysql_user_repository.hpp"
#include "application/auth_service.hpp"
#include "interface/user_service_impl.hpp"
#include "common/restful/http_server.hpp"
#include "interface/rest_api_handler.hpp"
#include "common/config/config.hpp"
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

    auto email_sender = std::make_shared<user_service::SMTPEmailQueue>();

    

    // 初始化认证服务
    auto auth_service = std::make_shared<user_service::AuthService>(
      repository, email_sender
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
    
    auto api_handler = std::make_shared<user_service::RestApiHandler>(auth_service);
    common::HttpServer http_server{ioc, http_endpoint, api_handler};
    
    std::cout << "HTTP Server listening on " << service_config.host << ":" << http_port << std::endl;
    
    // 在单独线程运行grpc服务器
    std::thread grpc_thread([&grpc_server]() {
      grpc_server->Wait();
    });

    http_server.run();
    ioc.run();

    // 主线程运行HTTP服务器
    
    ioc.stop();

    if (grpc_thread.joinable()) {
      grpc_thread.join();
    }
    
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}