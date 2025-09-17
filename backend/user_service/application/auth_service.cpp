#include "auth_service.hpp"
#include "common/config/config.hpp"
#include "common/connection_pool/redis_connection_pool.hpp"
#include <cassert>
#include <chrono>
#include <expected>
#include <format>
#include <hiredis/hiredis.h>
#include <openssl/rand.h>
#include <string>

namespace user_service {

std::expected<std::string, std::string> AuthService::sendAndSaveEmailVerificationCode(const std::string& email, const std::string& type) {
  if (!std::regex_match(email, email_pattern_)){
    return std::unexpected("Email format invalid");
  }
  if (type == "register"){
    if (repository_->findByEmail(email)){
      return std::unexpected("Email already exists");
    }
  }
  auto res_code = generateVerificationCode(email);
  if (!res_code){
    return std::unexpected("failed to generate verification code: " + res_code.error());
  }
  std::string code = res_code.value();
  auto res = sendEmailVerificationCode(email, code);
  if (!res){
    return std::unexpected("failed to send verification code: " + res.error());
  }else{
    res = saveVerificationCodeToRedis(email, code, type);
    if (!res){
      return std::unexpected("failed to save verification code: " + res.error());
    }
    return code;
  }
}

std::expected<std::string, std::string> AuthService::generateVerificationCode(const std::string& email){
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

std::expected<void, std::string> AuthService::saveVerificationCodeToRedis(const std::string& email, const std::string& code, const std::string& type){
  common::RedisConnectionGuard conn_guard(common::RedisConnectionPool::getInstance());
  std::string command = std::format("SETEX {}:email_vericode:{} 300 {}", type, email, code);
  redisReply *reply = (redisReply*)redisCommand(conn_guard.get(), command.c_str());

  if (reply == nullptr) {
    return std::unexpected("redisCommand returned nullptr. Connection may be lost.");
  }

  bool success = false;
  std::string error_msg;

  if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
    success = true;
  } else if (reply->type == REDIS_REPLY_ERROR) {
    error_msg = "Redis error: " + std::string(reply->str, reply->len);
  } else {
    error_msg = "Unexpected Redis reply type: " + std::to_string(reply->type);
  }

  freeReplyObject(reply);

  if (success) {
    return {};
  } else {
    return std::unexpected(error_msg);
  }
}

std::expected<std::string, std::string> AuthService::loadVerificationCodeFromRedis(const std::string& email, const std::string& type){
  common::RedisConnectionGuard conn_guard(common::RedisConnectionPool::getInstance());
  std::string command = std::format("GET {}:email_vericode:{}", type, email);
  redisReply *reply = (redisReply*)redisCommand(conn_guard.get(), command.c_str());

  if (reply == nullptr) {
    return std::unexpected("redisCommand returned nullptr. Connection may be lost.");
  }

  std::expected<std::string, std::string> result;
  if (reply->type == REDIS_REPLY_NIL) {
    result = std::unexpected("no verification code for email: " + email);
  } else if (reply->type == REDIS_REPLY_STRING) {
    result = std::string(reply->str, reply->len);
  } else if (reply->type == REDIS_REPLY_ERROR) {
    result = std::unexpected("Redis error: " + std::string(reply->str, reply->len));
  } else {
    result = std::unexpected("Unexpected Redis reply type: " + std::to_string(reply->type));
  }

  freeReplyObject(reply);
  return result;
}

std::expected<std::tuple<std::string, User>, std::string> AuthService::registerAndStore(const std::string& email,
                                                                                       const std::string& vericode,
                                                                                       const std::string& username,
                                                                                       const std::string& password,
                                                                                       const std::string& avatar) {
  if (repository_->findByEmail(email)){
    return std::unexpected("Email already exists");
  }
  auto code_res = loadVerificationCodeFromRedis(email, "register");
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

std::expected<std::tuple<std::string, User>, std::string> AuthService::loginEmailVeriCode(const std::string& email,
                                                                                         const std::string& code) {
  auto code_res = loadVerificationCodeFromRedis(email, "login");
  if (!code_res){
    if (code_res.error() == "no verification code for email"){
      return std::unexpected("verification code not prepared");
    }
    return std::unexpected("failed to get verification code from database: " + code_res.error());
  }
  if (code_res.value() != code){
    return std::unexpected("verification code wrong");
  }

  auto user_opt = repository_->findByEmail(email);
  if (!user_opt) {
    return std::unexpected("Invalid email");
  }

  auto user = user_opt.value();
  auto token = createToken(user.id());
  return std::make_tuple(token, user);
}

std::expected<std::tuple<std::string, User>, std::string> AuthService::loginEmailPwd(const std::string& email,
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
    .set_issuer("jmanime_user_service")
    .set_type("JWT")
    .set_issued_now()
    .set_expires_in(std::chrono::seconds(3600*auth.jwt_expire_hours))
    .set_payload_claim("user_id", jwt::claim(user_id))
    .sign(jwt::algorithm::hs256{auth.jwt_secret});
}


std::expected<void, std::string> AuthService::sendEmailVerificationCode(const std::string& email, const std::string& code) {
  if (!email_sender_) {
    return std::unexpected("Email sender not available");
  }
  
  if (!email_sender_->available()) {
    return std::unexpected("Email queue is full");
  }
  
  std::string subject = "Email Verification Code";
  std::string body = "Your verification code is: " + code;
  
  auto future = email_sender_->addTask(email, subject, body);
  
  // 等待邮件发送结果（可以设置超时）
  if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
    bool success = future.get();
    if (success) {
      return {};
    } else {
      return std::unexpected("Failed to send verification email");
    }
  } else {
    return std::unexpected("Email sending timeout");
  }
}
}