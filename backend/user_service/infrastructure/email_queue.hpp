#pragma once
#include "common/config/config.hpp"
#include "domain/email_sender.hpp"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <expected>
#include <future>
#include <mutex>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <openssl/err.h>

namespace user_service {

struct EmailTask {
  std::atomic_bool ready;
  std::string to_email;
  std::string subject;
  std::string body;
  std::chrono::steady_clock::time_point created_at;
  std::promise<bool> prom;
  
  EmailTask(const std::string& email, const std::string& subj, const std::string& content)
    : to_email(email), subject(subj), body(content), ready(false),
      created_at(std::chrono::steady_clock::now()) {}

  ~EmailTask(){
    if (prom.get_future().wait_for(std::chrono::seconds(0)) == std::future_status::deferred)
      prom.set_value(false);
  }
};

// 场景：单个消费者，消费任务；多个生产者，生产任务
class SMTPEmailQueue final: public EmailSender{
private:
  config::SMTPConfig cfg_;
  size_t capacity_;
  std::mutex mtx;
  std::condition_variable condition_;
  std::atomic_bool stop_{false};
  std::jthread thread_;
  std::allocator<EmailTask> alloc_;
  EmailTask* data_;
  // (pos=Capacity) new task (pos=tail) >> tail >> head >> worker (pos=0)
  // head和tail只会增加，tail位置还没有元素
  alignas(64) size_t head_{0};
  alignas(64) std::atomic<size_t> tail_{0};
  
  // SMTP连接相关
  boost::asio::io_context io_context_;
  boost::asio::ssl::context ssl_context_;
  std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket_;
  bool connected_{false};
  
public:
  SMTPEmailQueue();
  ~SMTPEmailQueue();

  void stop();

  bool available() override;
  bool empty() override;

  std::future<bool> addTask(const std::string& to_email, const std::string& subject, const std::string& body) override;
  void loop();

private:
  bool ensureConnection();
  bool connect();
  void disconnect();
  bool performSMTPHandshake();
  int readResponse();

  std::string base64_encode(const std::string& in);
  std::expected<void, std::string> sendEmail(EmailTask& task);

};
}