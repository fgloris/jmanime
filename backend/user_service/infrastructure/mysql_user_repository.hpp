#pragma once
#include "domain/user_repository.hpp"
#include <mysql/mysql.h>
#include <string>
#include <cstring>
#include <vector>

namespace user_service {
class MysqlUserRepository : public UserRepository {
public:
  explicit MysqlUserRepository();
  ~MysqlUserRepository() = default;
  
  bool save(const User& user) override;
  std::optional<User> findById(const std::string& id) override;
  std::optional<User> findByEmail(const std::string& email) override;

private:
  // 执行查询并获取单个用户结果
  std::optional<User> executeSelectQuery(const char* query, const std::string& param);
};



// RAII wrapper for MySQL bind parameters
class MysqlBindHelper {
public:
  MysqlBindHelper(size_t param_count) : binds_(param_count), string_storage_(param_count) {
    std::memset(binds_.data(), 0, sizeof(MYSQL_BIND) * param_count);
  }
  
  void bind_string(size_t pos, const std::string& str) {
    if (pos >= binds_.size()) return;
    
    string_storage_[pos] = str;
    
    binds_[pos].buffer_type = MYSQL_TYPE_STRING;
    binds_[pos].buffer = const_cast<char*>(string_storage_[pos].c_str());
    binds_[pos].buffer_length = string_storage_[pos].length();
  }
  
  MYSQL_BIND* data() { return binds_.data(); }
  
private:
  std::vector<MYSQL_BIND> binds_;
  std::vector<std::string> string_storage_;
};
}