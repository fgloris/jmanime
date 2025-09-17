#include "connection_pool.hpp"
namespace common {

ConnectionPool::~ConnectionPool() {
  shutdown_.store(true);
  condition_.notify_all();
  
  std::lock_guard<std::mutex> lock(mutex_);
  while (!pool_.empty()) {
    pool_.pop();
  }
}

bool ConnectionPool::validateConnection(Connection* conn) {
  return conn && conn->isValid();
}

std::unique_ptr<Connection> ConnectionPool::getConnectionFromPool() {
  std::unique_lock<std::mutex> lock(mutex_);
  
  // 等待连接可用或超时
  auto deadline = std::chrono::steady_clock::now() + cp_config_.timeout;
  
  while (pool_.empty() && active_connections_.load() >= cp_config_.max_connections && !shutdown_.load()) {
    if (condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
      throw std::runtime_error("Connection pool timeout");
    }
  }
  
  if (shutdown_.load()) {
    throw std::runtime_error("Connection pool is shutting down");
  }
  
  std::unique_ptr<Connection> conn;
  
  while (!pool_.empty()) {
    conn = std::move(pool_.front());
    pool_.pop();
    if (validateConnection(conn.get())) {
      break;
    }
    std::cout<<"here!!"<<std::endl;
    conn.reset();
  }
  
  // 如果没有有效连接且未达到最大连接数，创建新连接
  if (!conn && active_connections_.load() < cp_config_.max_connections) {
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

void ConnectionPool::returnConnection(std::unique_ptr<Connection> conn) {
  if (!conn) return;
  std::lock_guard<std::mutex> lock(mutex_);
  
  active_connections_.fetch_sub(1);
  
  if (shutdown_.load() || !validateConnection(conn.get()) || pool_.size() >= cp_config_.min_connections) {
    conn.reset();
  } else {
    pool_.push(std::move(conn));
  }
  condition_.notify_one();
}

size_t ConnectionPool::availableConnections() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pool_.size() + (cp_config_.max_connections - active_connections_.load());
}

void ConnectionPool::cleanupIdleConnections() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  while (pool_.size() > cp_config_.min_connections) {
    pool_.pop();
  }
}
  
}