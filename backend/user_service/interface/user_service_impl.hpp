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
    auto result = auth_service_->registerAndStore(
      request->email(),
      request->username(),
      request->password(),
      request->avatar()
    );

    if (!result) {
      response->set_success(false);
      response->set_message(result.error());
      return grpc::Status::OK;
    }

    auto [token, user_id] = result.value();
    response->set_success(true);
    response->set_user_id(user_id);
    response->set_token(token);
    return grpc::Status::OK;
  }

  grpc::Status Login(grpc::ServerContext* context,
                    const LoginRequest* request,
                    LoginResponse* response) override {
    auto result = auth_service_->login(
      request->email(),
      request->password()
    );

    if (!result) {
      response->set_success(false);
      response->set_message(result.error());
      return grpc::Status::OK;
    }

    auto [token, user] = result.value();
    response->set_success(true);
    response->set_token(token);
    response->set_user_id(user.id());
    response->set_username(user.username());
    response->set_avatar(user.avatar());
    return grpc::Status::OK;
  }

  grpc::Status ValidateToken(grpc::ServerContext* context,
                            const ValidateTokenRequest* request,
                            ValidateTokenResponse* response) override {
    auto result = auth_service_->validateToken(request->token());
    
    if (!result) {
      response->set_valid(false);
      return grpc::Status::OK;
    }

    response->set_valid(true);
    response->set_user_id(result.value());
    return grpc::Status::OK;
  }

private:
  std::shared_ptr<AuthService> auth_service_;
};
}