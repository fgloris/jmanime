#include "redis_connection_pool.hpp"
#include "common/config/config.hpp"
#include "common/connection_pool/connection_pool.hpp"
#include <cstring>
#include <hiredis/hiredis.h>

namespace common {

RedisConnection& RedisConnection::operator=(RedisConnection&& other) noexcept {
  if (this != &other) {
    if (conn_) {
      redisFree(conn_);
    }
    conn_ = other.conn_;
    other.conn_ = nullptr;
  }
  return *this;
}

bool RedisConnection::isValid() const {
  if (!conn_) return false;
  redisReply* reply = (redisReply*)redisCommand(conn_, "ping");
  bool result = !std::strcmp(reply->str, "PONG");
  freeReplyObject(reply);
  return result;
}

RedisConnectionPool::RedisConnectionPool() : ConnectionPool(config::Config::getInstance().getDBCntPool()), redis_config_(config::Config::getInstance().getRedis()) {
  for (size_t i = 0; i < cp_config_.min_connections; ++i) {
    auto conn = createConnection();
    if (conn) {
      pool_.push(std::move(conn));
    }
  }
}

std::unique_ptr<Connection> RedisConnectionPool::createConnection() {
  redisContext* conn = redisConnect(redis_config_.host.c_str(), redis_config_.port);
  if (conn == NULL || conn->err) {
    return nullptr;
  }
  
  return std::make_unique<RedisConnection>(conn);
}

} // namespace common