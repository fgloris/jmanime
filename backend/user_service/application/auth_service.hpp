#pragma once
#include <string>
#include <vector>
#include <jwt-cpp/jwt.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "domain/user.hpp"
#include "domain/user_repository.hpp"

namespace user_service {
class AuthService {
public:
  AuthService(std::shared_ptr<UserRepository> repository, const std::string& jwt_secret)
    : repository_(repository), jwt_secret_(jwt_secret) {}

  std::pair<bool, std::string> register_user(const std::string& email,
                                           const std::string& username,
                                           const std::string& password,
                                           const std::string& avatar) {
    if (repository_->findByEmail(email)) {
      return {false, "Email already exists"};
    }

    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);

    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
      return {false, "Failed to generate salt"};
    }

    std::string salt_str;
    for (int i = 0; i < 16; i++) {
      char hex[3];
      snprintf(hex, sizeof(hex), "%02x", salt[i]);
      salt_str += hex;
    }

    std::string salted_password = password + salt_str;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, salted_password.c_str(), salted_password.length());
    SHA256_Final(hash, &sha256);

    std::string hash_str;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
      char hex[3];
      snprintf(hex, sizeof(hex), "%02x", hash[i]);
      hash_str += hex;
    }

    User user(uuid_str, email, username, hash_str, salt_str, avatar);
    if (!repository_->save(user)) {
      return {false, "Failed to save user"};
    }

    auto token = create_token(uuid_str);
    return {true, token};
  }

  std::tuple<bool, std::string, User> login(const std::string& email,
                                          const std::string& password) {
    auto user_opt = repository_->findByEmail(email);
    if (!user_opt) {
      return {false, "Invalid email or password", User("", "", "", "", "", "")};
    }

    auto user = user_opt.value();
    std::string salted_password = password + user.salt();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, salted_password.c_str(), salted_password.length());
    SHA256_Final(hash, &sha256);

    std::string hash_str;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
      char hex[3];
      snprintf(hex, sizeof(hex), "%02x", hash[i]);
      hash_str += hex;
    }

    if (hash_str != user.password_hash()) {
      return {false, "Invalid email or password", User("", "", "", "", "", "")};
    }

    auto token = create_token(user.id());
    return {true, token, user};
  }

  std::pair<bool, std::string> validate_token(const std::string& token) {
    try {
      auto decoded = jwt::decode(token);
      auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{jwt_secret_});
      verifier.verify(decoded);
      
      auto user_id = decoded.get_payload_claim("user_id").as_string();
      return {true, user_id};
    } catch (std::exception&) {
      return {false, ""};
    }
  }

private:
  std::shared_ptr<UserRepository> repository_;
  std::string jwt_secret_;

  std::string create_token(const std::string& user_id) {
    return jwt::create()
      .set_issuer("auth0")
      .set_type("JWS")
      .set_issued_at(std::chrono::system_clock::now())
      .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24))
      .set_payload_claim("user_id", jwt::claim(user_id))
      .sign(jwt::algorithm::hs256{jwt_secret_});
  }
};
