#pragma once
#include <string>
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

  std::tuple<bool, std::string, std::string> registerAndStore(const std::string& email,
                                                   const std::string& username,
                                                   const std::string& password,
                                                   const std::string& avatar);

  std::tuple<bool, std::string, User> login(const std::string& email,
                                          const std::string& password);

  std::pair<bool, std::string> validateToken(const std::string& token);

private:
  std::shared_ptr<UserRepository> repository_;
  std::string jwt_secret_;

  std::string createToken(const std::string& user_id);
};
}