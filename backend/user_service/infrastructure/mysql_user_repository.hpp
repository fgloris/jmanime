#pragma once
#include "domain/user_repository.hpp"
#include <mysql/mysql.h>
#include <string>

namespace user_service {
class MysqlUserRepository : public UserRepository {
public:
  MysqlUserRepository(const std::string& host, const std::string& user,
                     const std::string& password, const std::string& database);
  ~MysqlUserRepository();
  
  bool save(const User& user) override;
  std::optional<User> findById(const std::string& id) override;
  std::optional<User> findByEmail(const std::string& email) override;

private:
  MYSQL* conn_;
};
}