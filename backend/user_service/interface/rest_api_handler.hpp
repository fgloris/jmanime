#pragma once
#include "application/auth_service.hpp"
#include "common/restful/rest_api_handler_base.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace user_service {

class RestApiHandler : public common::RestApiHandlerBase {
public:
  RestApiHandler(std::shared_ptr<AuthService> auth_service);

protected:
  http::response<http::string_body> doHandleRequest(
      http::request<http::string_body,
                    http::basic_fields<std::allocator<char>>> &&req) override;

private:
  std::shared_ptr<AuthService> auth_service_;

  http::response<http::string_body>
  handleRegisterValidateEmail(const nlohmann::json &body);
  http::response<http::string_body> handleRegister(const nlohmann::json &body);
  http::response<http::string_body>
  handleLoginEmailPwd(const nlohmann::json &body);
  http::response<http::string_body>
  handleLoginEmailCode(const nlohmann::json &body);
  http::response<http::string_body>
  handleLoginValidateEmail(const nlohmann::json &body);
  http::response<http::string_body>
  handleValidateToken(const std::string &token);
};



}