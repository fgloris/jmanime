#include "mysql_user_repository.hpp"
#include "common/config/config.hpp"
#include "common/connection_pool/mysql_connection_pool.hpp"
#include <cassert>
#include <cppconn/prepared_statement.h>
#include <uuid/uuid.h>
#include <cstring>

namespace user_service {

MysqlUserRepository::MysqlUserRepository() {
  auto db_config = config::Config::getInstance().getDatabase();
}

bool MysqlUserRepository::save(const User& user) {
  const char* query = "INSERT INTO users (id, email, username, password_hash, salt, avatar) "
                     "VALUES (?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE "
                     "email=?, username=?, password_hash=?, salt=?, avatar=?";

  common::MySQLConnectionGuard conn_guard(common::MySQLConnectionPool::getInstance());

  MYSQL_STMT* stmt = mysql_stmt_init(conn_guard.get());
  if (!stmt) {
    return false;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return false;
  }

  MysqlBindHelper bind_helper(11);
  bind_helper.bind_string(0, user.id());
  bind_helper.bind_string(1, user.email());
  bind_helper.bind_string(2, user.username());
  bind_helper.bind_string(3, user.password_hash());
  bind_helper.bind_string(4, user.salt());
  bind_helper.bind_string(5, user.avatar());
  
  // ON DUPLICATE KEY UPDATE parameters
  bind_helper.bind_string(6, user.email());
  bind_helper.bind_string(7, user.username());
  bind_helper.bind_string(8, user.password_hash());
  bind_helper.bind_string(9, user.salt());
  bind_helper.bind_string(10, user.avatar());

  if (mysql_stmt_bind_param(stmt, bind_helper.data())) {
    mysql_stmt_close(stmt);
    return false;
  }

  bool success = !mysql_stmt_execute(stmt);
  mysql_stmt_close(stmt);
  return success;
}

std::optional<User> MysqlUserRepository::findById(const std::string& id) {
  const char* query = "SELECT id, email, username, password_hash, salt, avatar "
                     "FROM users WHERE id = ?";
  
  common::MySQLConnectionGuard conn_guard(common::MySQLConnectionPool::getInstance());
  MYSQL_STMT* stmt = mysql_stmt_init(conn_guard.get());
  if (!stmt) {
    return std::nullopt;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MysqlBindHelper bind_helper(1);
  bind_helper.bind_string(0, id);

  if (mysql_stmt_bind_param(stmt, bind_helper.data())) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  if (mysql_stmt_execute(stmt)) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MYSQL_BIND result[6];
  memset(result, 0, sizeof(result));

  char id_buffer[37];
  char email_buffer[256];
  char username_buffer[256];
  char password_hash_buffer[65];
  char salt_buffer[33];
  char avatar_buffer[400*400*3];
  unsigned long id_length, email_length, username_length, 
                password_hash_length, salt_length, avatar_length;

  result[0].buffer_type = MYSQL_TYPE_STRING;
  result[0].buffer = id_buffer;
  result[0].buffer_length = sizeof(id_buffer);
  result[0].length = &id_length;

  result[1].buffer_type = MYSQL_TYPE_STRING;
  result[1].buffer = email_buffer;
  result[1].buffer_length = sizeof(email_buffer);
  result[1].length = &email_length;

  result[2].buffer_type = MYSQL_TYPE_STRING;
  result[2].buffer = username_buffer;
  result[2].buffer_length = sizeof(username_buffer);
  result[2].length = &username_length;

  result[3].buffer_type = MYSQL_TYPE_STRING;
  result[3].buffer = password_hash_buffer;
  result[3].buffer_length = sizeof(password_hash_buffer);
  result[3].length = &password_hash_length;

  result[4].buffer_type = MYSQL_TYPE_STRING;
  result[4].buffer = salt_buffer;
  result[4].buffer_length = sizeof(salt_buffer);
  result[4].length = &salt_length;

  result[5].buffer_type = MYSQL_TYPE_STRING;
  result[5].buffer = avatar_buffer;
  result[5].buffer_length = sizeof(avatar_buffer);
  result[5].length = &avatar_length;

  if (mysql_stmt_bind_result(stmt, result)) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  if (mysql_stmt_fetch(stmt) != 0) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  mysql_stmt_close(stmt);

  return User(
    std::string(id_buffer, id_length),
    std::string(email_buffer, email_length),
    std::string(username_buffer, username_length),
    std::string(password_hash_buffer, password_hash_length),
    std::string(salt_buffer, salt_length),
    std::string(avatar_buffer, avatar_length)
  );
}

std::optional<User> MysqlUserRepository::findByEmail(const std::string& email) {
  const char* query = "SELECT id, email, username, password_hash, salt, avatar "
                     "FROM users WHERE email = ?";
  
  common::MySQLConnectionGuard conn_guard(common::MySQLConnectionPool::getInstance());
  MYSQL_STMT* stmt = mysql_stmt_init(conn_guard.get());
  if (!stmt) {
    return std::nullopt;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MysqlBindHelper bind_helper(1);
  bind_helper.bind_string(0, email);

  if (mysql_stmt_bind_param(stmt, bind_helper.data())) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  if (mysql_stmt_execute(stmt)) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MYSQL_BIND result[6];
  memset(result, 0, sizeof(result));

  char id_buffer[37];
  char email_buffer[256];
  char username_buffer[256];
  char password_hash_buffer[65];
  char salt_buffer[33];
  char avatar_buffer[400*400*3];
  unsigned long id_length, email_length, username_length, 
                password_hash_length, salt_length, avatar_length;

  result[0].buffer_type = MYSQL_TYPE_STRING;
  result[0].buffer = id_buffer;
  result[0].buffer_length = sizeof(id_buffer);
  result[0].length = &id_length;

  result[1].buffer_type = MYSQL_TYPE_STRING;
  result[1].buffer = email_buffer;
  result[1].buffer_length = sizeof(email_buffer);
  result[1].length = &email_length;

  result[2].buffer_type = MYSQL_TYPE_STRING;
  result[2].buffer = username_buffer;
  result[2].buffer_length = sizeof(username_buffer);
  result[2].length = &username_length;

  result[3].buffer_type = MYSQL_TYPE_STRING;
  result[3].buffer = password_hash_buffer;
  result[3].buffer_length = sizeof(password_hash_buffer);
  result[3].length = &password_hash_length;

  result[4].buffer_type = MYSQL_TYPE_STRING;
  result[4].buffer = salt_buffer;
  result[4].buffer_length = sizeof(salt_buffer);
  result[4].length = &salt_length;

  result[5].buffer_type = MYSQL_TYPE_STRING;
  result[5].buffer = avatar_buffer;
  result[5].buffer_length = sizeof(avatar_buffer);
  result[5].length = &avatar_length;

  if (mysql_stmt_bind_result(stmt, result)) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  if (mysql_stmt_fetch(stmt) != 0) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  mysql_stmt_close(stmt);

  return User(
    std::string(id_buffer, id_length),
    std::string(email_buffer, email_length),
    std::string(username_buffer, username_length),
    std::string(password_hash_buffer, password_hash_length),
    std::string(salt_buffer, salt_length),
    std::string(avatar_buffer, avatar_length)
  );
}
}