#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "infrastructure/mysql_user_repository.hpp"
#include "application/auth_service.hpp"
#include "interface/user_service_impl.hpp"

int main(int argc, char** argv) {
  // 服务配置
  std::string server_address("0.0.0.0:50051");
  std::string jwt_secret("your-secret-key");
  
  // 数据库配置
  std::string db_host("localhost");
  std::string db_user("root");
  std::string db_password("114472988");
  std::string db_name("user_db");

  try {
    // 初始化存储层
    auto repository = std::make_shared<user_service::MysqlUserRepository>(
      db_host, db_user, db_password, db_name
    );

    // 初始化认证服务
    auto auth_service = std::make_shared<user_service::AuthService>(
      repository, jwt_secret
    );

    // 初始化gRPC服务
    user_service::UserServiceImpl service(auth_service);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}