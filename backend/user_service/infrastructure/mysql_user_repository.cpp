#include "mysql_user_repository.hpp"
#include <cppconn/prepared_statement.h>
#include <uuid/uuid.h>

namespace user_service {
MysqlUserRepository::MysqlUserRepository(const std::string& host, const std::string& user,
                                       const std::string& password, const std::string& database) {
  conn_ = mysql_init(nullptr);
  if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                         database.c_str(), 0, nullptr, 0)) {
    throw std::runtime_error(mysql_error(conn_));
  }
}

MysqlUserRepository::~MysqlUserRepository() {
  if (conn_) {
    mysql_close(conn_);
  }
}

bool MysqlUserRepository::save(const User& user) {
  const char* query = "INSERT INTO users (id, email, username, password_hash, salt, avatar) "
                     "VALUES (?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE "
                     "email=?, username=?, password_hash=?, salt=?, avatar=?";
  
  MYSQL_STMT* stmt = mysql_stmt_init(conn_);
  if (!stmt) {
    return false;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return false;
  }

  MYSQL_BIND bind[11];
  memset(bind, 0, sizeof(bind));

  bind[0].buffer_type = MYSQL_TYPE_STRING;
  bind[0].buffer = (void*)user.id().c_str();
  bind[0].buffer_length = user.id().length();

  bind[1].buffer_type = MYSQL_TYPE_STRING;
  bind[1].buffer = (void*)user.email().c_str();
  bind[1].buffer_length = user.email().length();

  bind[2].buffer_type = MYSQL_TYPE_STRING;
  bind[2].buffer = (void*)user.username().c_str();
  bind[2].buffer_length = user.username().length();

  bind[3].buffer_type = MYSQL_TYPE_STRING;
  bind[3].buffer = (void*)user.password_hash().c_str();
  bind[3].buffer_length = user.password_hash().length();

  bind[4].buffer_type = MYSQL_TYPE_STRING;
  bind[4].buffer = (void*)user.salt().c_str();
  bind[4].buffer_length = user.salt().length();

  bind[5].buffer_type = MYSQL_TYPE_STRING;
  bind[5].buffer = (void*)user.avatar().c_str();
  bind[5].buffer_length = user.avatar().length();

  // For ON DUPLICATE KEY UPDATE
  bind[6] = bind[1];
  bind[7] = bind[2];
  bind[8] = bind[3];
  bind[9] = bind[4];
  bind[10] = bind[5];

  if (mysql_stmt_bind_param(stmt, bind)) {
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
  
  MYSQL_STMT* stmt = mysql_stmt_init(conn_);
  if (!stmt) {
    return std::nullopt;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MYSQL_BIND bind[1];
  memset(bind, 0, sizeof(bind));

  bind[0].buffer_type = MYSQL_TYPE_STRING;
  bind[0].buffer = (void*)id.c_str();
  bind[0].buffer_length = id.length();

  if (mysql_stmt_bind_param(stmt, bind)) {
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
  char avatar_buffer[512];
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
  
  MYSQL_STMT* stmt = mysql_stmt_init(conn_);
  if (!stmt) {
    return std::nullopt;
  }

  if (mysql_stmt_prepare(stmt, query, strlen(query))) {
    mysql_stmt_close(stmt);
    return std::nullopt;
  }

  MYSQL_BIND bind[1];
  memset(bind, 0, sizeof(bind));

  bind[0].buffer_type = MYSQL_TYPE_STRING;
  bind[0].buffer = (void*)email.c_str();
  bind[0].buffer_length = email.length();

  if (mysql_stmt_bind_param(stmt, bind)) {
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
  char avatar_buffer[512];
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
