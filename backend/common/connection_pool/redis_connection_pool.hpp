#pragma once

#include "common/config/config.hpp"
#include "common/connection_pool/connection_pool.hpp"
#include <hiredis/hiredis.h>
#include <memory>

namespace common {

class RedisConnection : public Connection {
public:
  RedisConnection();
  RedisConnection(redisContext* conn): conn_(conn) {}
  ~RedisConnection() override { if (conn_) redisFree(conn_); }
  
  redisContext* get() const { return conn_; }
  bool isValid() const override;

  RedisConnection(RedisConnection&& other): conn_(other.conn_) {other.conn_ = nullptr; }
  RedisConnection& operator=(RedisConnection&& other) noexcept;

private:
  redisContext* conn_ = nullptr;
};


class RedisConnectionPool final : public ConnectionPool{
public:
  static RedisConnectionPool& getInstance() {
    static RedisConnectionPool instance;
    return instance;
  }
  std::unique_ptr<Connection> createConnection() override;

private:
  RedisConnectionPool();
  config::RedisConfig redis_config_;
};

class RedisConnectionGuard final : public ConnectionGuard{
  using ConnectionGuard::ConnectionGuard;
public:
  redisContext* get() const {
    return static_cast<RedisConnection*>(conn_.get())->get();
  }

};
} // namespace common