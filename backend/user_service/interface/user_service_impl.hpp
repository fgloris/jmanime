#pragma once
#include <memory>
#include <grpcpp/grpcpp.h>
#include "application/auth_service.hpp"
#include "../build/generated/user.grpc.pb.h"

namespace user_service {
class UserServiceImpl final : public UserService::Service {
public:
  UserServiceImpl(std::shared_ptr<AuthService> auth_service)
    : auth_service_(auth_service) {}

  grpc::Status Register(grpc::ServerContext* context,
                       const RegisterRequest* request,
                       RegisterResponse* response) override {
    auto [success, message, user_id] = auth_service_->registerAndStore(
      request->email(),
      request->username(),
      request->password(),
      request->avatar()
    );

    response->set_success(success);
    if (success) {
      response->set_user_id(user_id);
      response->set_token(message);  // message 在成功时包含 token
    } else {
      response->set_message(message);
    }
    return grpc::Status::OK;
  }

  grpc::Status Login(grpc::ServerContext* context,
                    const LoginRequest* request,
                    LoginResponse* response) override {
    auto [success, token, user] = auth_service_->login(
      request->email(),
      request->password()
    );

    response->set_success(success);
    if (success) {
      response->set_token(token);
      response->set_user_id(user.id());
      response->set_username(user.username());
      response->set_avatar(user.avatar());
    } else {
      response->set_message(token);
    }
    return grpc::Status::OK;
  }

  grpc::Status ValidateToken(grpc::ServerContext* context,
                            const ValidateTokenRequest* request,
                            ValidateTokenResponse* response) override {
    auto [valid, user_id] = auth_service_->validateToken(request->token());
    response->set_valid(valid);
    if (valid) {
      response->set_user_id(user_id);
    }
    return grpc::Status::OK;
  }

private:
  std::shared_ptr<AuthService> auth_service_;
};
}