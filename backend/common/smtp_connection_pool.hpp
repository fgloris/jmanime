#pragma once

#include "common/config.hpp"
#include "common/connection_pool.hpp"
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/system/detail/error_code.hpp>
#include <expected>

namespace common {

class SMTPConnection : public Connection {
public:
  SMTPConnection();
  SMTPConnection(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket);
  ~SMTPConnection() override;

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket>* get() const { return socket_.get(); }

  bool isValid() const override;

  std::expected<void, std::string> authenticate(const std::string& username, const std::string& password);
  std::expected<void, std::string> sendMail(const std::string& from, const std::string& to,
                                          const std::string& subject, const std::string& body);

  SMTPConnection(SMTPConnection&& other) noexcept : socket_(std::move(other.socket_)) {}
  SMTPConnection& operator=(SMTPConnection&& other) noexcept;

private:
  std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket_;
  boost::asio::io_context* io_context_;

  std::expected<void, std::string> readResponse(const std::string& expected_code);
  std::expected<void, std::string> sendCommand(const std::string& command);
};

class SMTPConnectionPool final : public ConnectionPool {
public:
  SMTPConnectionPool();

protected:
  std::unique_ptr<Connection> createConnection() override;

private:
  config::SMTPConfig smtp_config_;
};

class SMTPConnectionGuard final : public ConnectionGuard {
public:
  using ConnectionGuard::ConnectionGuard; // 继承构造函数

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket>* get() const {
    return static_cast<SMTPConnection*>(conn_.get())->get();
  }

  std::expected<void, std::string> sendMail(const std::string& from, const std::string& to,
                                          const std::string& subject, const std::string& body) {
    return static_cast<SMTPConnection*>(conn_.get())->sendMail(from, to, subject, body);
  }
};

} // namespace common