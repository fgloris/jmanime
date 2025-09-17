#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <iostream>

#include "common/config/config.hpp"

namespace common {

// RAII: 构造建立一个mysql连接, 析构时自动结束连接
class Connection {
public:
  virtual ~Connection() {std::cout<<"base destroyed!"<<std::endl;};
  virtual bool isValid() const = 0;
  
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;
  
  Connection() = default;
  Connection(Connection&&) = default;
  Connection& operator=(Connection&&) = default;
};


/*
  池子内连接数 = 空闲可用的连接数 (更新时使 池子内连接数 <= min_connections)
  活跃连接数 = 正在被外部使用的连接数
  总连接数 = 池子内连接数 + 活跃连接数 <= max_connections
*/
class ConnectionPool {
public:
~ConnectionPool();

std::unique_ptr<Connection> getConnectionFromPool();

void returnConnection(std::unique_ptr<Connection> conn);

size_t activeConnections() const { return active_connections_; }
size_t availableConnections() const;

ConnectionPool(const ConnectionPool&) = delete;
ConnectionPool& operator=(const ConnectionPool&) = delete;

protected:
  ConnectionPool(const config::ConnectionPoolConfig& cfg): cp_config_(cfg){};
  virtual std::unique_ptr<Connection> createConnection() = 0;

  bool validateConnection(Connection* conn);

  void cleanupIdleConnections();

  config::ConnectionPoolConfig cp_config_;
  std::queue<std::unique_ptr<Connection>> pool_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<size_t> active_connections_{0};
  std::atomic<bool> shutdown_{false};
};

// RAII: 构造时从连接池获取一个连接, 析构时自动将连接归还给连接池
class ConnectionGuard {
public:
  ConnectionGuard(ConnectionPool& pool) : pool_(pool) { conn_ = pool_.getConnectionFromPool(); }
  ~ConnectionGuard() { if (conn_) pool_.returnConnection(std::move(conn_)); }
  
  Connection* operator->() const { return conn_.get(); }
  Connection& operator*() const { return *conn_; }
  
  bool valid() const { return conn_ != nullptr; }
  
  ConnectionGuard(const ConnectionGuard&) = delete;
  ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    
protected:
  ConnectionPool& pool_;
  std::unique_ptr<Connection> conn_;
};

} // namespace common