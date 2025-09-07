#include "connection_pool.hpp"
#include "common/config.hpp"
#include <stdexcept>

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

ConnectionPool::ConnectionPool() {
  db_config_ = config::Config::getInstance().getDatabase();
  for (size_t i = 0; i < db_config_.min_connections; ++i) {
    auto conn = createConnection();
    if (conn) {
      pool_.push(std::move(conn));
    }
  }
}

ConnectionPool::~ConnectionPool() {
  shutdown_.store(true);
  condition_.notify_all();
  
  std::lock_guard<std::mutex> lock(mutex_);
  while (!pool_.empty()) {
    pool_.pop();
  }
}

std::unique_ptr<MySQLConnection> ConnectionPool::createConnection() {
  MYSQL* conn = mysql_init(nullptr);
  if (!conn) {
    return nullptr;
  }
  
  mysql_options(conn, MYSQL_SET_CHARSET_NAME, db_config_.charset.c_str());
  
  unsigned int timeout = static_cast<unsigned int>(db_config_.timeout.count() / 1000);
  mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
  
  // 启用自动重连
  bool reconnect = 1;
  mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
  
  if (!mysql_real_connect(conn, db_config_.host.c_str(), db_config_.user.c_str(),
                        db_config_.password.c_str(), db_config_.db_name.c_str(),
                        db_config_.port, nullptr, 0)) {
    mysql_close(conn);
    return nullptr;
  }
  
  return std::make_unique<MySQLConnection>(conn);
}

bool ConnectionPool::validateConnection(MySQLConnection* conn) {
  return conn && conn->isValid();
}

std::unique_ptr<MySQLConnection> ConnectionPool::getConnectionFromPool() {
  std::unique_lock<std::mutex> lock(mutex_);
  
  // 等待连接可用或超时
  auto deadline = std::chrono::steady_clock::now() + db_config_.timeout;
  
  while (pool_.empty() && active_connections_.load() >= db_config_.max_connections && !shutdown_.load()) {
    if (condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
      throw std::runtime_error("Connection pool timeout");
    }
  }
  
  if (shutdown_.load()) {
    throw std::runtime_error("Connection pool is shutting down");
  }
  
  std::unique_ptr<MySQLConnection> conn;
  
  // 优先使用池中的连接
  while (!pool_.empty()) {
    conn = std::move(pool_.front());
    pool_.pop();
    
    // 验证连接是否有效
    if (validateConnection(conn.get())) {
      break;
    }
    
    conn.reset();
  }
  
  // 如果没有有效连接且未达到最大连接数，创建新连接
  if (!conn && active_connections_.load() < db_config_.max_connections) {
    lock.unlock();
    conn = createConnection();
    lock.lock();
    
    if (!conn) {
      throw std::runtime_error("Failed to create database connection");
    }
  }
  
  if (conn) {
    active_connections_.fetch_add(1);
  }
  
  return conn;
}

void ConnectionPool::releaseConnection(std::unique_ptr<MySQLConnection> conn) {
  if (!conn) return;
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  active_connections_.fetch_sub(1);
  
  if (shutdown_.load() || !validateConnection(conn.get())) {
    // 连接无效或正在关闭，直接销毁
    conn.reset();
  } else if (pool_.size() < db_config_.min_connections) {
    // 池中连接数少于最小值，归还到池中
    pool_.push(std::move(conn));
  } else {
    // 池中连接数足够，直接销毁
    conn.reset();
  }
  
  condition_.notify_one();
}

size_t ConnectionPool::availableConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pool_.size() + (db_config_.max_connections - active_connections_.load());
}

void ConnectionPool::cleanupIdleConnections() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  // 保持最小连接数，清理多余连接
  while (pool_.size() > db_config_.min_connections) {
    pool_.pop();
  }
}

} // namespace common