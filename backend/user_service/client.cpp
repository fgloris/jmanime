#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "../build/generated/user.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace user_service;

class UserServiceClient {
public:
  UserServiceClient(std::shared_ptr<Channel> channel)
    : stub_(UserService::NewStub(channel)) {}

  std::string Register(const std::string& email, const std::string& username,
               const std::string& password, const std::string& avatar) {
    RegisterRequest request;
    request.set_email(email);
    request.set_username(username);
    request.set_password(password);
    request.set_avatar(avatar);

    RegisterResponse response;
    ClientContext context;

    Status status = stub_->Register(&context, request, &response);

    if (status.ok()) {
      if (response.success()) {
        std::cout << "Registration successful!" << std::endl;
        std::cout << "message: " << response.message() << std::endl;
        std::cout << "User ID: " << response.user_id() << std::endl;
        std::cout << "Token: " << response.token() << std::endl;
        return response.token();
      } else {
        std::cout << "Registration failed: " << response.message() << std::endl;
      }
    } else {
      std::cout << "RPC failed: " << status.error_message() << std::endl;
    }
    return "";
  }

  std::string Login(const std::string& email, const std::string& password) {
    LoginRequest request;
    request.set_email(email);
    request.set_password(password);

    LoginResponse response;
    ClientContext context;

    Status status = stub_->Login(&context, request, &response);

    if (status.ok()) {
      if (response.success()) {
        std::cout << "Login successful!" << std::endl;
        std::cout << "User ID: " << response.user_id() << std::endl;
        std::cout << "Username: " << response.username() << std::endl;
        std::cout << "Token: " << response.token() << std::endl;
        return response.token();
      } else {
        std::cout << "Login failed: " << response.message() << std::endl;
      }
    } else {
      std::cout << "RPC failed: " << status.error_message() << std::endl;
    }
    return "";
  }

  bool ValidateToken(const std::string& token) {
    ValidateTokenRequest request;
    request.set_token(token);

    ValidateTokenResponse response;
    ClientContext context;

    Status status = stub_->ValidateToken(&context, request, &response);

    if (status.ok()) {
      if (response.valid()) {
        std::cout << "Token is valid!" << std::endl;
        std::cout << "User ID: " << response.user_id() << std::endl;
        return true;
      } else {
        std::cout << "Token is invalid!" << std::endl;
      }
    } else {
      std::cout << "RPC failed: " << status.error_message() << std::endl;
    }
    return false;
  }

private:
  std::unique_ptr<UserService::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_str("localhost:50051");
  UserServiceClient client(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials())
  );

  std::string token;

  // Test registration
  if (!client.Register("19313199238@163.com", "testuser", "1234566", "avatar.jpg").empty()) {
    // Test login
    if (const auto token = client.Login("19313199238@163.com", "1234567");!token.empty()) {
      // Get token from login response and validate it
      // Note: In real usage, you would save the token from the response
      client.ValidateToken(token);
    }
  }

  return 0;
}
