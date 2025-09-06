#include "auth_service.hpp"
#include "common/config.hpp"
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace user_service {

std::expected<std::tuple<std::string, User>, std::string> AuthService::registerAndStore(const std::string& email,
                                                   const std::string& username,
                                                   const std::string& password,
                                                   const std::string& avatar) {
  auto res = sendEmailVerificationCode(email, "123456");
  return std::unexpected("test ends!");
  if (!std::regex_match(email, email_pattern)){
    return std::unexpected("Email format invalid");
  }
  if (repository_->findByEmail(email)) {
    return std::unexpected("Email already exists");
  }
  if (!res){
    std::cout<<res.error()<<std::endl;
  }else{
    std::cout<<"send email success!"<<std::endl;
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
    
    const auto& smtpConfig = config::Config::getInstance().getSMTP();
    using namespace boost::asio;
    io_context io_context;

    ssl::context ssl_context(ssl::context::tlsv13);
    ssl_context.set_default_verify_paths();
    ssl_context.set_verify_mode(ssl::verify_peer);

    ip::tcp::resolver resolver(io_context);
    ip::tcp::resolver::results_type endpoints = resolver.resolve(smtpConfig.server, std::to_string(smtpConfig.port));
    
    //ip::tcp::socket socket(io_context);
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
      std::cout << "Server response: \n" << full_response << std::endl;
      return success;
    };

    // 1. 等待服务器的欢迎消息 (220)
    if (!read_response("220")) {
        return std::unexpected("Failed to receive welcome message");
    }

    // 2. 发送 EHLO 命令
    std::string ehlo_cmd = "EHLO " + smtpConfig.server + "\r\n";
    write(socket, buffer(ehlo_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to send EHLO command");
    }

    // 3. 发送 AUTH LOGIN 命令
    std::string auth_login_cmd = "AUTH LOGIN\r\n";
    write(socket, buffer(auth_login_cmd));
    if (!read_response("334")) {
        return std::unexpected("Failed to send AUTH LOGIN command");
    }

    // 4. 发送 Base64 编码的用户名
    std::string username_base64 = base64_encode(smtpConfig.from_email);
    write(socket, buffer(username_base64 + "\r\n"));
    if (!read_response("334")) {
        return std::unexpected("Failed to send username");
    }

    // 5. 发送 Base64 编码的密码
    std::string password_base64 = base64_encode(smtpConfig.password);
    write(socket, buffer(password_base64 + "\r\n"));
    if (!read_response("235")) {
        return std::unexpected("Failed to authenticate");
    }

    // 6. 发送 MAIL FROM 命令
    std::string mail_from_cmd = "MAIL FROM:<" + smtpConfig.from_email + ">\r\n";
    write(socket, buffer(mail_from_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to set sender");
    }

    // 7. 发送 RCPT TO 命令
    std::string rcpt_to_cmd = "RCPT TO:<" + email + ">\r\n";
    write(socket, buffer(rcpt_to_cmd));
    if (!read_response("250")) {
        return std::unexpected("Failed to set recipient");
    }

    // 8. 发送 DATA 命令
    std::string data_cmd = "DATA\r\n";
    write(socket, buffer(data_cmd));
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
    write(socket, buffer(email_body + "\r\n.\r\n"));
    if (!read_response("250")) {
        return std::unexpected("Failed to send email body");
    }

    // 10. 发送 QUIT 命令并关闭连接
    std::string quit_cmd = "QUIT\r\n";
    write(socket, buffer(quit_cmd));
    read_response("221");

    socket.lowest_layer().shutdown(ip::tcp::socket::shutdown_both);
    socket.lowest_layer().close();
    
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to send verification email: ") + e.what());
  }
}
}