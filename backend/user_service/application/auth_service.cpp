#include "auth_service.hpp"
#include "common/config.hpp"
#include "common/smtp_connection_pool.hpp"
#include <cassert>
#include <chrono>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <memory>
#include <string>

namespace user_service {

std::expected<std::string, std::string> AuthService::registerSendEmailVerificationCode(const std::string& email) {
  if (!std::regex_match(email, email_pattern)){
    return std::unexpected("Email format invalid");
  }
  auto res_code = generateVerificationCode(email);
  if (!res_code){
    return std::unexpected("failed to generate verification code: " + res_code.error());
  }
  std::string code = res_code.value();
  auto res = sendEmailVerificationCode(email, code);
  if (!res){
    std::cout<<res.error()<<std::endl;
    return std::unexpected("failed to send verification code: " + res.error());
  }else{
    res = saveVerificationCodeToDB(email, code);
    if (!res){
      return std::unexpected("failed to save verification code: " + res.error());
    }
    return code;
  }
}

std::expected<std::string, std::string> AuthService::generateVerificationCode(const std::string& email){
  return "123456";
  const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const int code_length = 8;
  
  unsigned char random_bytes[code_length];
  if (int res = RAND_bytes(random_bytes, code_length) != 1) {
    return std::unexpected("openssl func RAND_bytes error: " + std::to_string(res));
  }
  
  std::string code;
  code.reserve(code_length);
  
  for (int i = 0; i < code_length; i++) {
    code += chars[random_bytes[i] % chars.length()];
  }
  
  return code;
}

std::expected<void, std::string> AuthService::saveVerificationCodeToDB(const std::string& email, const std::string& code){
  return {};
}

std::expected<std::string, std::string> AuthService::loadVerificationCodeFromDB(const std::string& email){
  return "123456";
}

std::expected<std::tuple<std::string, User>, std::string> AuthService::registerAndStore(const std::string& email,
                                                                                       const std::string& vericode,
                                                                                       const std::string& username,
                                                                                       const std::string& password,
                                                                                       const std::string& avatar) {
  auto code_res = loadVerificationCodeFromDB(email);
  if (!code_res){
    if (code_res.error() == "no verification code for email"){
      return std::unexpected("verification code not prepared");
    }
    return std::unexpected("failed to get verification code from database: " + code_res.error());
  }
  if (code_res.value() != vericode){
    return std::unexpected("verification code wrong");
  }
  uuid_t uuid;
  uuid_generate(uuid);
  char uuid_str[37];
  uuid_unparse(uuid, uuid_str);

  unsigned char salt[16];
  if (RAND_bytes(salt, sizeof(salt)) != 1) {
    return std::unexpected("Failed to generate salt");
  }

  std::string salt_str;
  for (int i = 0; i < 16; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", salt[i]);
    salt_str += hex;
  }

  std::string salted_password = password + salt_str;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  
  auto ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, salted_password.c_str(), salted_password.length());
  EVP_DigestFinal_ex(ctx, hash, &hash_len);
  EVP_MD_CTX_free(ctx);

  std::string hash_str;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", hash[i]);
    hash_str += hex;
  }

  User user(uuid_str, email, username, hash_str, salt_str, avatar);
  if (!repository_->save(user)) {
    return std::unexpected("Failed to save user");
  }

  auto token = createToken(uuid_str);
  return std::make_tuple(token, user);
}

std::expected<std::tuple<std::string, User>, std::string> AuthService::login(const std::string& email,
                                                                            const std::string& password) {
  auto user_opt = repository_->findByEmail(email);
  if (!user_opt) {
    return std::unexpected("Invalid email or password");
  }

  auto user = user_opt.value();
  std::string salted_password = password + user.salt();
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  
  auto ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, salted_password.c_str(), salted_password.length());
  EVP_DigestFinal_ex(ctx, hash, &hash_len);
  EVP_MD_CTX_free(ctx);

  std::string hash_str;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", hash[i]);
    hash_str += hex;
  }

  if (hash_str != user.password_hash()) {
    return std::unexpected("Invalid email or password");
  }

  auto token = createToken(user.id());
  return std::make_tuple(token, user);
}

std::expected<std::string, std::string> AuthService::validateToken(const std::string& token) {
  try {
    auto decoded = jwt::decode(token);
    auto verifier = jwt::verify()
      .allow_algorithm(jwt::algorithm::hs256{config::Config::getInstance().getAuth().jwt_secret});
    verifier.verify(decoded);
    
    auto user_id = decoded.get_payload_claim("user_id").as_string();
    return user_id;
  } catch (std::exception&) {
    return std::unexpected("Invalid token");
  }
}

std::string AuthService::createToken(const std::string& user_id) {
  const auto& auth = config::Config::getInstance().getAuth();
  return jwt::create()
    .set_issuer("auth0")
    .set_type("JWT")
    .set_issued_now()
    .set_expires_in(std::chrono::seconds(3600*auth.jwt_expire_hours))
    .set_payload_claim("user_id", jwt::claim(user_id))
    .sign(jwt::algorithm::hs256{auth.jwt_secret});
}

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

std::expected<void, std::string> AuthService::sendEmailVerificationCode(const std::string& email, const std::string& code){

  try {
    auto start = std::chrono::steady_clock::now();
    const auto& smtpConfig = config::Config::getInstance().getSMTP();
    using namespace boost::asio;
    io_context io_context;

    ssl::context ssl_context(ssl::context::tlsv13);
    ssl_context.set_default_verify_paths();
    ssl_context.set_verify_mode(ssl::verify_peer);

    ip::tcp::resolver resolver(io_context);
    ip::tcp::resolver::results_type endpoints = resolver.resolve(smtpConfig.server, std::to_string(smtpConfig.port));
    
    auto socket = std::make_shared<ssl::stream<ip::tcp::socket>>(io_context, ssl_context);

    connect(socket->lowest_layer(), endpoints);
    socket->handshake(boost::asio::ssl::stream_base::client);

    streambuf response;
    std::string response_line;

    auto read_response = [&](const std::string& expected_code) -> bool {
      std::string response_line;
      std::string full_response;
      bool success = false;
      do {
        read_until(*socket, response, "\r\n");
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
      std::cout << "Server response: \n" << full_response << std::endl;
      return success;
    };

    // 1. 等待服务器的欢迎消息 (220)
    if (!read_response("220")) {
        return std::unexpected("Failed to receive welcome message");
    }

    // 2. 发送 EHLO 命令
    std::string ehlo_cmd = "EHLO " + smtpConfig.server + "\r\n";
    write(*socket, buffer(ehlo_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to send EHLO command");
    }

    // 3. 发送 AUTH LOGIN 命令
    std::string auth_login_cmd = "AUTH LOGIN\r\n";
    write(*socket, buffer(auth_login_cmd));
    if (!read_response("334")) {
        return std::unexpected("Failed to send AUTH LOGIN command");
    }

    // 4. 发送 Base64 编码的用户名
    std::string username_base64 = base64_encode(smtpConfig.from_email);
    write(*socket, buffer(username_base64 + "\r\n"));
    if (!read_response("334")) {
        return std::unexpected("Failed to send username");
    }

    // 5. 发送 Base64 编码的密码
    std::string password_base64 = base64_encode(smtpConfig.password);
    write(*socket, buffer(password_base64 + "\r\n"));
    if (!read_response("235")) {
        return std::unexpected("Failed to authenticate");
    }

    // 6. 发送 MAIL FROM 命令
    std::string mail_from_cmd = "MAIL FROM:<" + smtpConfig.from_email + ">\r\n";
    write(*socket, buffer(mail_from_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to set sender");
    }

    // 7. 发送 RCPT TO 命令
    std::string rcpt_to_cmd = "RCPT TO:<" + email + ">\r\n";
    write(*socket, buffer(rcpt_to_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to set recipient");
    }

    // 8. 发送 DATA 命令
    std::string data_cmd = "DATA\r\n";
    write(*socket, buffer(data_cmd));
    if (!read_response("354")) {
        return std::unexpected("Failed to send DATA command");
    }

    // 9. 发送邮件内容
    std::string email_body = "From: " + smtpConfig.from_email + "\r\n"
                          + "To: " + email + "\r\n"
                          + "Subject: Email Verification Code\r\n"
                          + "MIME-Version: 1.0\r\n"
                          + "Content-Type: text/plain; charset=utf-8\r\n"
                          + "\r\n"
                          + "Your verification code is: " + code + "\r\n";
    write(*socket, buffer(email_body + "\r\n.\r\n"));
    if (!read_response("250")) {
        return std::unexpected("Failed to send email body");
    }

    // 10. 发送 QUIT 命令并关闭连接
    std::string quit_cmd = "QUIT\r\n";
    write(*socket, buffer(quit_cmd));
    read_response("221");

    // 不等待对方服务器相应的关闭SSL连接方式
    // 正确关闭SSL连接 - 不等待对端close_notify响应的关闭方式
    boost::system::error_code shutdown_ec, write_ec;
    
    // 启动异步shutdown操作，发送一个close_notify，并关闭本侧SSL写入端，异步等待另一侧服务器的close_notify
    socket->async_shutdown([&shutdown_ec, socket](const boost::system::error_code& ec) {
        shutdown_ec = ec;
    });
    
    // 启动一个异步写操作，用于在本侧SSL写入端关闭时触发底层tcp连接关闭
    const char dummy_buffer[] = "\0";
    async_write(*socket, buffer(dummy_buffer, 1), 
                [&write_ec, socket](const boost::system::error_code& ec, std::size_t) {
      write_ec = ec;
      // 检查是否是SSL协议关闭错误，如果是，代表async_shutdown已经关闭本侧SSL写入端
      if ((ec.category() == boost::asio::error::get_ssl_category()) &&
          (SSL_R_PROTOCOL_IS_SHUTDOWN == ERR_GET_REASON(ec.value()))) {
          // 关闭底层tcp连接，这会取消shutdown操作对另一侧的close_notify的等待
          socket->lowest_layer().close();
      }
    });

    // 运行io_context直到所有操作完成或被取消
    io_context.run();

    socket.reset();
    auto end = std::chrono::steady_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout<<cost.count()<<std::endl;
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to send verification email: ") + e.what());
  }
}
}