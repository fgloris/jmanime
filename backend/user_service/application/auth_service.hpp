#pragma once
#include <string>
#include <expected>
#include <jwt-cpp/jwt.h>
#include <uuid.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "domain/user.hpp"
#include "domain/user_repository.hpp"

namespace user_service {
class AuthService {
public:
  AuthService(std::shared_ptr<UserRepository> repository, const std::string& jwt_secret)
    : repository_(repository), jwt_secret_(jwt_secret) {}

  std::expected<std::tuple<std::string, std::string>, std::string> registerAndStore(const std::string& email,
                                                   const std::string& username,
                                                   const std::string& password,
                                                   const std::string& avatar);

  std::expected<std::tuple<std::string, User>, std::string> login(const std::string& email,
                                          const std::string& password);

  std::expected<std::string, std::string> validateToken(const std::string& token);

private:
  std::shared_ptr<UserRepository> repository_;
  std::string jwt_secret_;

  std::string createToken(const std::string& user_id);
};
}