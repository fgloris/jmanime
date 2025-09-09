#pragma once
#include <optional>
#include "domain/user.hpp"

namespace user_service {
class UserRepository {
public:
  virtual ~UserRepository() = default;
  virtual bool save(const User& user) = 0;
  virtual std::optional<User> findById(const std::string& id) = 0;
  virtual std::optional<User> findByEmail(const std::string& email) = 0;
};
}