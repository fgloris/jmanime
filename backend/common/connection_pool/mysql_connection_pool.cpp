#include "mysql_connection_pool.hpp"
#include "common/config/config.hpp"
#include "common/connection_pool/connection_pool.hpp"

namespace common {

MySQLConnection& MySQLConnection::operator=(MySQLConnection&& other) noexcept {
  if (this != &other) {
    if (conn_) {
      mysql_close(conn_);
    }
    conn_ = other.conn_;
    other.conn_ = nullptr;
  }
  return *this;
}

bool MySQLConnection::isValid() const {
  if (!conn_) return false;
  return mysql_ping(conn_) == 0;
}

MySQLConnectionPool::MySQLConnectionPool() : ConnectionPool(config::Config::getInstance().getDBCntPool()), db_config_(config::Config::getInstance().getDatabase()) {
  for (size_t i = 0; i < cp_config_.min_connections; ++i) {
    auto conn = createConnection();
    if (conn) {
      pool_.push(std::move(conn));
    }
  }
}

std::unique_ptr<Connection> MySQLConnectionPool::createConnection() {
  MYSQL* conn = mysql_init(nullptr);
  if (!conn) {
    return nullptr;
  }
  
  mysql_options(conn, MYSQL_SET_CHARSET_NAME, db_config_.charset.c_str());
  
  unsigned int timeout = static_cast<unsigned int>(cp_config_.timeout.count() / 1000);
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
  
  if (!mysql_real_connect(conn, db_config_.host.c_str(), db_config_.user.c_str(),
                        db_config_.password.c_str(), db_config_.db_name.c_str(),
                        db_config_.port, nullptr, 0)) {
    mysql_close(conn);
    return nullptr;
  }
  
  return std::make_unique<MySQLConnection>(conn);
}

} // namespace common