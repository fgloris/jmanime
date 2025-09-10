#pragma once
#include <string>
#include <expected>
#include <regex>
#include <jwt-cpp/jwt.h>
#include <uuid.h>
#include "domain/user.hpp"
#include "domain/user_repository.hpp"
#include "domain/email_sender.hpp"

namespace user_service {
class AuthService {
public:
  AuthService(std::shared_ptr<UserRepository> repository, std::shared_ptr<EmailSender> email_sender)
    : repository_(repository), email_sender_(email_sender),
      email_pattern_("^[a-zA-Z0-9_+&*-]+(?:\\.[a-zA-Z0-9_+&*-]+)*" \
	"@(?:[a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,7}$") {}

  // return token, user struct
  std::expected<std::tuple<std::string, User>, std::string> registerAndStore(const std::string& email,
                                                                            const std::string& verify_code,
                                                                            const std::string& username,
                                                                            const std::string& password,
                                                                            const std::string& avatar);

  std::expected<std::string, std::string> sendAndSaveEmailVerificationCode(const std::string& email, const std::string& type);

  // return token, user struct
  std::expected<std::tuple<std::string, User>, std::string> loginEmailPwd(const std::string& email,
                                                                         const std::string& password);

  std::expected<std::tuple<std::string, User>, std::string> loginEmailVeriCode(const std::string& email,
                                                                              const std::string& code);

  // return user_id
  std::expected<std::string, std::string> validateToken(const std::string& token);

  std::expected<void, std::string> sendEmailVerificationCode(const std::string& email, const std::string& code);
private:
  std::expected<std::string, std::string> generateVerificationCode(const std::string& email);
  std::expected<void, std::string> saveVerificationCodeToRedis(const std::string& email, const std::string& code, const std::string& type);
  std::expected<std::string, std::string> loadVerificationCodeFromRedis(const std::string& email, const std::string& type);

  std::shared_ptr<UserRepository> repository_;
  std::shared_ptr<EmailSender> email_sender_;
  
  std::string createToken(const std::string& user_id);
  std::regex email_pattern_;
};
}