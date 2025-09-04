#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "infrastructure/mysql_user_repository.hpp"
#include "application/auth_service.hpp"
#include "interface/user_service_impl.hpp"
#include "common/config.hpp"
#include <sstream>

int main(int argc, char** argv) {
  try {
    const auto& cfg = config::Config::getInstance();
    const auto& db_config = cfg.getDatabase();
    const auto& service_config = cfg.getUserService();
    const auto& auth_config = cfg.getAuth();
    
    // 构建服务地址
    std::stringstream server_address;
    server_address << service_config.host << ":" << service_config.port;

    // 初始化存储层
    auto repository = std::make_shared<user_service::MysqlUserRepository>(
      db_config.host,
      db_config.user,
      db_config.password,
      db_config.name
    );

    // 初始化认证服务
    auto auth_service = std::make_shared<user_service::AuthService>(
      repository
    );

    // 初始化gRPC服务
    user_service::UserServiceImpl service(auth_service);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address.str(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address.str() << std::endl;

    server->Wait();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}