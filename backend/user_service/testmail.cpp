#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <print>
#include "common/config/config.hpp"
#include "infrastructure/mysql_user_repository.hpp"
#include <iostream>

std::string base64_encode(const std::string& in) {
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


int main(){
  const auto& cfg = config::Config::getInstance();
  const auto& db_config = cfg.getDatabase();
  const auto& smtpConfig = cfg.getSMTP();
  std::string email = "19902512605@163.com";
  std::string code = "123456";

  auto repository = std::make_shared<user_service::MysqlUserRepository>(
      db_config.host,
      db_config.user,
      db_config.password,
      db_config.db_name
    );

  if (repository->findByEmail(email)){
    std::cout<<"tag1"<<std::endl;
  }else{
    std::cout<<"tag2"<<std::endl;
  }

  
  using namespace boost::asio;
  io_context io_context;

  ssl::context ssl_context(ssl::context::tlsv13);
  ssl_context.set_default_verify_paths();
  ssl_context.set_verify_mode(ssl::verify_peer);

  ip::tcp::resolver resolver(io_context);
  ip::tcp::resolver::results_type endpoints = resolver.resolve(smtpConfig.server, std::to_string(smtpConfig.port));
  
  ssl::stream<ip::tcp::socket> socket(io_context, ssl_context);
  connect(socket.lowest_layer(), endpoints);


  socket.handshake(boost::asio::ssl::stream_base::client);


  streambuf response;
  std::string response_line;

  auto read_response = [&](const std::string& expected_code) -> bool {
    std::string response_line;
    std::string full_response;
    bool success = false;
    do {
      read_until(socket, response, "\r\n");
      std::istream is(&response);
      std::getline(is, response_line);
      full_response += response_line + "\n";
      
      if (response_line.length() >= 4 && response_line.at(3) == '-') {
        continue;
      } else {
        success = response_line.substr(0, 3) == expected_code;
        break;
      }
    } while (true);
    std::print("Server response: \n{}\n", full_response);
    return success;
  };

  // 1. 等待服务器的欢迎消息 (220)
  if (!read_response("220")) {
    std::print("Failed to receive welcome message");
  }

  // 2. 发送 EHLO 命令
  std::string ehlo_cmd = "EHLO " + smtpConfig.server + "\r\n";
  write(socket, buffer(ehlo_cmd));
  if (!read_response("250")) {
    std::print("Failed to send EHLO command");
  }

  // 3. 发送 AUTH LOGIN 命令
  std::string auth_login_cmd = "AUTH LOGIN\r\n";
  write(socket, buffer(auth_login_cmd));
  if (!read_response("334")) {
    std::print("Failed to send AUTH LOGIN command");
  }

  // 4. 发送 Base64 编码的用户名
  std::string username_base64 = base64_encode(smtpConfig.from_email);
  write(socket, buffer(username_base64 + "\r\n"));
  if (!read_response("334")) {
    std::print("Failed to send username");
  }

  // 5. 发送 Base64 编码的密码
  std::string password_base64 = base64_encode(smtpConfig.password);
  write(socket, buffer(password_base64 + "\r\n"));
  if (!read_response("235")) {
    std::print("Failed to authenticate");
  }

  // 6. 发送 MAIL FROM 命令
  std::string mail_from_cmd = "MAIL FROM:<" + smtpConfig.from_email + ">\r\n";
  write(socket, buffer(mail_from_cmd));
  if (!read_response("250")) {
    std::print("Failed to set sender");
  }

  // 7. 发送 RCPT TO 命令
  std::string rcpt_to_cmd = "RCPT TO:<" + email + ">\r\n";
  write(socket, buffer(rcpt_to_cmd));
  if (!read_response("250")) {
    std::print("Failed to set recipient");
  }

  // 8. 发送 DATA 命令
  std::string data_cmd = "DATA\r\n";
  write(socket, buffer(data_cmd));
  if (!read_response("354")) {
    std::print("Failed to send DATA command");
  }

  // 9. 发送邮件内容
  std::string email_body = "From: " + smtpConfig.from_email + "\r\n"
                        + "To: " + email + "\r\n"
                        + "Subject: Email Verification Code\r\n"
                        + "MIME-Version: 1.0\r\n"
                        + "Content-Type: text/plain; charset=utf-8\r\n"
                        + "\r\n"
                        + "Your verification code is: " + code + "\r\n";
  write(socket, buffer(email_body + "\r\n.\r\n"));
  if (!read_response("250")) {
    std::print("Failed to send email body");
  }

  // 10. 发送 QUIT 命令并关闭连接
  std::string quit_cmd = "QUIT\r\n";
  write(socket, buffer(quit_cmd));
  read_response("221");

  socket.lowest_layer().shutdown(ip::tcp::socket::shutdown_both);
  socket.lowest_layer().close();
  return 0;
}
