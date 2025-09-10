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

  grpc::Status ValidateEmail(grpc::ServerContext* context,
                            const ValidateEmailRequest* request,
                            ValidateEmailResponse* response) override {
    auto result = auth_service_->sendAndSaveEmailVerificationCode(request->email(), request->type());
    
    response->set_send_code_success(result.has_value());
    if (!result) {
      response->set_message(result.error());
    }
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