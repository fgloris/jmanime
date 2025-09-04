#pragma once
#include <string>

namespace user_service {
class User {
public:
  User(const std::string& id, const std::string& email, const std::string& username,
       const std::string& password_hash, const std::string& salt, const std::string& avatar)
    : id_(id), email_(email), username_(username), password_hash_(password_hash),
      salt_(salt), avatar_(avatar) {}
  
  User(){}

  std::string id() const { return id_; }
  std::string email() const { return email_; }
  std::string username() const { return username_; }
  std::string password_hash() const { return password_hash_; }
  std::string salt() const { return salt_; }
  std::string avatar() const { return avatar_; }

private:
  std::string id_;
  std::string email_;
  std::string username_;
  std::string password_hash_;
  std::string salt_;
  std::string avatar_;
};
}