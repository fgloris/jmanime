#include "auth_service.hpp"
#include "common/config.hpp"
#include <chrono>
#include <boost/asio.hpp>

namespace user_service {
std::expected<std::tuple<std::string, User>, std::string> AuthService::registerAndStore(const std::string& email,
                                                   const std::string& username,
                                                   const std::string& password,
                                                   const std::string& avatar) {
  if (repository_->findByEmail(email)) {
    return std::unexpected("Email already exists");
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

std::expected<void, std::string> AuthService::sendEmailVerificationCode(const std::string& email, const std::string& code){
  try {
    const auto& smtpConfig = config::Config::getInstance().getSMTP();
    
    
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to send verification email: ") + e.what());
  }
}
}