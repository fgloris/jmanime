#pragma once

#include "common/config/config.hpp"
#include "common/connection_pool/connection_pool.hpp"
#include <mysql/mysql.h>
#include <memory>
#include <iostream>
namespace common {

// RAII: 构造建立一个mysql连接, 析构时自动结束连接
class MySQLConnection : public Connection {
public:
  MySQLConnection();
  MySQLConnection(MYSQL* conn): conn_(conn) {}
  ~MySQLConnection() override { std::cout<<"mysql connection destroyed!"<<std::endl; if (conn_) mysql_close(conn_); }
  
  MYSQL* get() const { return conn_; }
  bool isValid() const override;

  MySQLConnection(MySQLConnection&& other): conn_(other.conn_) {other.conn_ = nullptr; }
  MySQLConnection& operator=(MySQLConnection&& other) noexcept;

private:
  MYSQL* conn_ = nullptr;
};


class MySQLConnectionPool final : public ConnectionPool{
public:
  static MySQLConnectionPool& getInstance() {
    static MySQLConnectionPool instance;
    return instance;
  }
  std::unique_ptr<Connection> createConnection() override;

private:
  MySQLConnectionPool();
  config::DatabaseConfig db_config_;
};

class MySQLConnectionGuard final : public ConnectionGuard{
  using ConnectionGuard::ConnectionGuard;
public:
  MYSQL* get() const {
    return static_cast<MySQLConnection*>(conn_.get())->get();
  }

};
} // namespace common