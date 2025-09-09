#pragma once
#include <string>
#include <expected>
#include <regex>
#include <jwt-cpp/jwt.h>
#include <uuid.h>
#include "domain/user.hpp"
#include "domain/user_repository.hpp"

namespace user_service {
class AuthService {
public:
  AuthService(std::shared_ptr<UserRepository> repository)
    : repository_(repository), email_pattern_("^[a-zA-Z0-9_+&*-]+(?:\\.[a-zA-Z0-9_+&*-]+)*" \
	"@(?:[a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,7}$") {}

  // return token, user struct
  std::expected<std::tuple<std::string, User>, std::string> registerAndStore(const std::string& email,
                                                                            const std::string& verify_code,
                                                                            const std::string& username,
                                                                            const std::string& password,
                                                                            const std::string& avatar);

  std::expected<std::string, std::string> registerSendEmailVerificationCode(const std::string& email);

  // return token, user struct
  std::expected<std::tuple<std::string, User>, std::string> login(const std::string& email,
                                          const std::string& password);

  // return user_id
  std::expected<std::string, std::string> validateToken(const std::string& token);

  std::expected<void, std::string> sendEmailVerificationCode(const std::string& email, const std::string& code);
private:
  std::expected<std::string, std::string> generateVerificationCode(const std::string& email);
  std::expected<void, std::string> saveVerificationCodeToDB(const std::string& email, const std::string& code);
  std::expected<std::string, std::string> loadVerificationCodeFromDB(const std::string& email);

  std::shared_ptr<UserRepository> repository_;
  std::string createToken(const std::string& user_id);
  std::regex email_pattern_;
};
}