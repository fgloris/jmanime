#include "email_queue.hpp"
#include <expected>
#include <memory>
#include <iostream>
#include <string>

namespace user_service {
  SMTPEmailQueue::SMTPEmailQueue() : cfg_(config::Config::getInstance().getSMTP()), capacity_(cfg_.queue_max_size),
    data_(alloc_.allocate(capacity_)), ssl_context_(boost::asio::ssl::context::tlsv13) {
    ssl_context_.set_default_verify_paths();
    ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
    thread_ = std::jthread([this]() { loop(); });
  }
  
  SMTPEmailQueue::~SMTPEmailQueue() { 
    stop(); 
    disconnect();
    alloc_.deallocate(data_, capacity_);
  }

  void SMTPEmailQueue::stop() {
    stop_.store(true, std::memory_order_release);
    condition_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  bool SMTPEmailQueue::available() {
    return (tail_.load(std::memory_order_acquire) + 1) % capacity_ != head_;
  }

  bool SMTPEmailQueue::empty() {
    return head_ == tail_.load(std::memory_order_acquire);
  }

  std::future<bool> SMTPEmailQueue::addTask(const std::string& to_email, const std::string& subject, const std::string& body){
    if (!available()) {
      std::promise<bool> p;
      p.set_value(false);
      return p.get_future();
    }
    
    size_t pos = tail_.load(std::memory_order_relaxed);
    while (!tail_.compare_exchange_weak(pos, (pos + 1)%capacity_ , std::memory_order_relaxed)){
      pos = tail_.load(std::memory_order_relaxed);
    }
    std::construct_at(&data_[pos], to_email, subject, body);
    data_[pos].ready.store(true, std::memory_order_release);
    condition_.notify_one();
    return data_[pos].prom.get_future();
  }

  void SMTPEmailQueue::loop(){
    while (true) {
      std::unique_lock<std::mutex> lock{mtx};
      condition_.wait(lock, [this]() -> bool {
        return stop_.load(std::memory_order_acquire) || 
          ((!empty()) && data_[head_].ready.load(std::memory_order_acquire));
      });// 等待条件变量被唤醒，若任务为空且线程池未被关闭则继续等待

      if (stop_.load(std::memory_order_acquire) && empty()) {
        break;
      }

      // 确保SMTP连接可用
      if (!ensureConnection()) {
        data_[head_].prom.set_value(false);
        head_ = (head_ + 1) % capacity_;
        continue;
      }

      auto ret = sendEmail(data_[head_]);
      bool success = ret.has_value();
      data_[head_].prom.set_value(success);
      head_ = (head_ + 1) % capacity_;
    }
  }

  bool SMTPEmailQueue::ensureConnection() {
    if (connected_ && socket_ && socket_->lowest_layer().is_open()) {
      return true;
    }
    return connect();
  }
  
  bool SMTPEmailQueue::connect() {
    try {
      boost::asio::ip::tcp::resolver resolver(io_context_);
      auto endpoints = resolver.resolve(cfg_.server, std::to_string(cfg_.port));
      
      socket_ = std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(io_context_, ssl_context_);
      
      boost::asio::connect(socket_->lowest_layer(), endpoints);
      socket_->handshake(boost::asio::ssl::stream_base::client);
      
      // SMTP握手
      if (!performSMTPHandshake()) {
        disconnect();
        return false;
      }
      
      connected_ = true;
      return true;
    } catch (const std::exception& e) {
      connected_ = false;
      return false;
    }
  }
  
  void SMTPEmailQueue::disconnect() {
    if (socket_ && socket_->lowest_layer().is_open()) {
      try {
        // 发送QUIT命令
        std::string quit_cmd = "QUIT\r\n";
        boost::asio::write(*socket_, boost::asio::buffer(quit_cmd));
        
        // 优雅关闭SSL连接
        boost::system::error_code shutdown_ec, write_ec;
    
        // 启动异步shutdown操作，发送一个close_notify，并关闭本侧SSL写入端，异步等待另一侧服务器的close_notify
        socket_->async_shutdown([&shutdown_ec, socket = this->socket_](const boost::system::error_code& ec) {
            shutdown_ec = ec;
        });
        
        // 启动一个异步写操作，用于在本侧SSL写入端关闭时触发底层tcp连接关闭
        const char dummy_buffer[] = "\0";
        boost::asio::async_write(*socket_, boost::asio::buffer(dummy_buffer, 1), 
                    [&write_ec, socket = this->socket_](const boost::system::error_code& ec, std::size_t) {
          write_ec = ec;
          // 检查是否是SSL协议关闭错误，如果是，代表async_shutdown已经关闭本侧SSL写入端
          if ((ec.category() == boost::asio::error::get_ssl_category()) &&
              (SSL_R_PROTOCOL_IS_SHUTDOWN == ERR_GET_REASON(ec.value()))) {
              // 关闭底层tcp连接，这会取消shutdown操作对另一侧的close_notify的等待
              socket->lowest_layer().close();
          }
        });
      } catch (...) {
        // 忽略关闭时的异常
      }
    }
    socket_.reset();
    connected_ = false;
  }
  
int SMTPEmailQueue::readResponse() {
    std::string response_line;
    boost::asio::streambuf response;
    std::string full_response;
    do {
      boost::asio::read_until(*socket_, response, "\r\n");
      std::istream is(&response);
      std::getline(is, response_line);
      full_response += response_line;
      
      if (response_line.length() >= 4 && response_line.at(3) == '-') {
        continue;
      } else {
        int result = std::stoi(response_line.substr(0, 3));
        return result;
      }
    } while (true);
  }

  bool SMTPEmailQueue::performSMTPHandshake() {
    try {
      
      // 等待服务器欢迎消息
      if (readResponse() != 220) return false;
      
      // EHLO
      std::string ehlo_cmd = "EHLO " + cfg_.server + "\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(ehlo_cmd));
      if (readResponse() != 250) return false;
      
      // AUTH LOGIN
      std::string auth_login_cmd = "AUTH LOGIN\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(auth_login_cmd));
      if (readResponse() != 334) return false;
      
      // 用户名
      std::string username_base64 = base64_encode(cfg_.from_email);
      boost::asio::write(*socket_, boost::asio::buffer(username_base64 + "\r\n"));
      if (readResponse() != 334) return false;
      
      // 密码
      std::string password_base64 = base64_encode(cfg_.password);
      boost::asio::write(*socket_, boost::asio::buffer(password_base64 + "\r\n"));
      if (readResponse() != 235) return false;
      
      return true;
    } catch (const std::exception& e) {
      return false;
    }
  }

  std::string SMTPEmailQueue::base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    for (char c : in) {
      val = (val << 8) + static_cast<unsigned char>(c);
      valb += 8;
      while (valb >= 0) {
        out.push_back(base64_chars[(val >> valb) & 0x3F]);
        valb -= 6;
      }
    }
    if (valb > -6) {
      out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
      out.push_back('=');
    }
    return out;
  }

  std::expected<void, std::string> SMTPEmailQueue::sendEmail(EmailTask& task) {
    if (!connected_ || !socket_) {
      return std::unexpected("connection not valid");
    }

    
    try {
      boost::asio::streambuf response;
      
      // MAIL FROM
      std::string mail_from_cmd = "MAIL FROM:<" + cfg_.from_email + ">\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(mail_from_cmd));
      int ret_code = readResponse();
      if (ret_code != 250) {
        if (ret_code == 451){
          return std::unexpected("Mail From message send failed, ret code:" + std::to_string(ret_code) + "smtp server refuses to recieve trash mails");
        }
        return std::unexpected("Mail From message send failed, ret code:" + std::to_string(ret_code));
      }
      
      // RCPT TO
      std::string rcpt_to_cmd = "RCPT TO:<" + task.to_email + ">\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(rcpt_to_cmd));
      if (int ret_code = readResponse() != 250) return std::unexpected("Rcpt To message send failed, ret code:" + std::to_string(ret_code));
      
      // DATA
      std::string data_cmd = "DATA\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(data_cmd));
      if (int ret_code = readResponse() != 354) return std::unexpected("Data message send failed, ret code:" + std::to_string(ret_code));
      
      // 邮件内容
      std::string email_body = "From: " + cfg_.email_sender_name + "\r\n"
                            + "To: " + task.to_email + "\r\n"
                            + "Subject: " + task.subject + "\r\n"
                            + "MIME-Version: 1.0\r\n"
                            + "Content-Type: text/plain; charset=utf-8\r\n"
                            + "\r\n"
                            + task.body + "\r\n";
      boost::asio::write(*socket_, boost::asio::buffer(email_body + "\r\n.\r\n"));
      if (int ret_code = readResponse() != 250) return std::unexpected("Email Body message send failed, ret code:" + std::to_string(ret_code));
      
      return {};
    } catch (const std::exception& e) {
      // 连接可能断开，标记为未连接
      connected_ = false;
      return std::unexpected("Connection error");
    }
  }
}