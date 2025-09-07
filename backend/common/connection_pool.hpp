#pragma once

#include "common/config.hpp"
#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

namespace common {

// RAII: 构造建立一个mysql连接, 析构时自动结束连接
struct MySQLConnection {
  MySQLConnection(MYSQL* conn): conn_(conn) {}
  ~MySQLConnection() { if (conn_) mysql_close(conn_); }
  
  MYSQL* get() const { return conn_; }
  bool isValid() const;
  
  MySQLConnection(const MySQLConnection&) = delete;
  MySQLConnection& operator=(const MySQLConnection&) = delete;
  
  MySQLConnection(MySQLConnection&& other): conn_(other.conn_) {other.conn_ = nullptr; }
  MySQLConnection& operator=(MySQLConnection&& other) noexcept;

  MYSQL* conn_;
};

class ConnectionPool {
public:
  ConnectionPool();
  ~ConnectionPool();
  
  std::unique_ptr<MySQLConnection> getConnectionFromPool();
  
  void releaseConnection(std::unique_ptr<MySQLConnection> conn);
  
  size_t activeConnections() const { return active_connections_; }
  size_t availableConnections() const;
  
  ConnectionPool(const ConnectionPool&) = delete;
  ConnectionPool& operator=(const ConnectionPool&) = delete;

private:
  config::DatabaseConfig db_config_;
  std::queue<std::unique_ptr<MySQLConnection>> pool_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<size_t> active_connections_{0};
  std::atomic<bool> shutdown_{false};
  
  std::unique_ptr<MySQLConnection> createConnection();
  
  bool validateConnection(MySQLConnection* conn);
  
  void cleanupIdleConnections();
};

// RAII: 构造时从连接池获取一个连接, 析构时自动将连接归还给连接池
class ConnectionGuard {
public:
  ConnectionGuard(ConnectionPool& pool) : pool_(pool) { conn_ = pool_.getConnectionFromPool(); }
  ~ConnectionGuard() { if (conn_) pool_.releaseConnection(std::move(conn_)); }
  
  MySQLConnection* operator->() const { return conn_.get(); }
  MySQLConnection& operator*() const { return *conn_; }
  MYSQL* get() const { return conn_->get(); }
  
  bool valid() const { return conn_ != nullptr; }
  
  ConnectionGuard(const ConnectionGuard&) = delete;
  ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    
private:
  ConnectionPool& pool_;
  std::unique_ptr<MySQLConnection> conn_;
};

} // namespace common