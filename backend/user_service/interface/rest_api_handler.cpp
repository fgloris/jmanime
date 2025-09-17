#include "rest_api_handler.hpp"

namespace user_service {

RestApiHandler::RestApiHandler(std::shared_ptr<AuthService> auth_service)
    : auth_service_(auth_service) {}

http::response<http::string_body> RestApiHandler::doHandleRequest(
    http::request<http::string_body,
                  http::basic_fields<std::allocator<char>>> &&req) {
  std::string target = std::string(req.target());

  if (target == "/api/auth/register-validate-email" &&
      req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleRegisterValidateEmail(body);
  } else if (target == "/api/auth/register" && req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleRegister(body);
  } else if (target == "/api/auth/login-email-pwd" &&
             req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleLoginEmailPwd(body);
  } else if (target == "/api/auth/login-email-code" &&
             req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleLoginEmailCode(body);
  } else if (target == "/api/auth/login-validate-email" &&
             req.method() == http::verb::post) {
    auto body = parseRequestBody(std::string(req.body()));
    return handleLoginValidateEmail(body);
  } else if (target == "/api/auth/validate-token" &&
             req.method() == http::verb::post) {
    auto auth_header = req[http::field::authorization];
    if (auth_header.empty()) {
      return createErrorResponse(http::status::unauthorized,
                                 "Missing authorization header");
    }

    std::string token = std::string(auth_header);
    if (token.starts_with("Bearer ")) {
      token = token.substr(7);
    }

    return handleValidateToken(token);
  } else {
    return createErrorResponse(http::status::not_found, "Endpoint not found");
  }
}

http::response<http::string_body>
RestApiHandler::handleRegisterValidateEmail(const nlohmann::json &body) {
  try {
    if (!body.contains("email")) {
      return createErrorResponse(http::status::bad_request,
                                 "Missing email field");
    }

    std::string email = body["email"];
    auto result =
        auth_service_->sendAndSaveEmailVerificationCode(email, "register");

    if (result) {
      nlohmann::json response_json = {
          {"success", true},
          {"message", "Verification code sent successfully"},
          {"code", result.value()} // 开发时返回验证码，生产环境应移除
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::bad_request, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(
        http::status::internal_server_error,
        "Failed to process email validation: " + std::string(e.what()));
  }
}

http::response<http::string_body>
RestApiHandler::handleLoginValidateEmail(const nlohmann::json &body) {
  try {
    if (!body.contains("email")) {
      return createErrorResponse(http::status::bad_request,
                                 "Missing email field");
    }

    std::string email = body["email"];
    auto result =
        auth_service_->sendAndSaveEmailVerificationCode(email, "login");

    if (result) {
      nlohmann::json response_json = {
          {"success", true},
          {"message", "Verification code sent successfully"},
          {"code", result.value()} // 开发时返回验证码，生产环境应移除
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::bad_request, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(
        http::status::internal_server_error,
        "Failed to process email validation: " + std::string(e.what()));
  }
}

http::response<http::string_body>
RestApiHandler::handleRegister(const nlohmann::json &body) {
  try {
    // 验证必要字段
    std::vector<std::string> required_fields = {"email", "verification_code",
                                              "username", "password"};
    for (const auto &field : required_fields) {
      if (!body.contains(field)) {
        return createErrorResponse(http::status::bad_request,
                                   "Missing field: " + field);
      }
    }

    std::string email = body["email"];
    std::string verification_code = body["verification_code"];
    std::string username = body["username"];
    std::string password = body["password"];
    std::string avatar = body.value("avatar", "");

    auto result = auth_service_->registerAndStore(email, verification_code,
                                                  username, password, avatar);

    if (result) {
      auto [token, user] = result.value();
      nlohmann::json response_json = {
          {"success", true},
          {"message", "User registered successfully"},
          {"token", token},
          {"user",
           {{"id", user.id()},
            {"email", user.email()},
            {"username", user.username()},
            {"avatar", user.avatar()}}},
      };
      return createJsonResponse(http::status::created, response_json);
    } else {
      return createErrorResponse(http::status::bad_request, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(
        http::status::internal_server_error,
        "Failed to process registration: " + std::string(e.what()));
  }
}

http::response<http::string_body>
RestApiHandler::handleLoginEmailPwd(const nlohmann::json &body) {
  try {
    if (!body.contains("email") || !body.contains("password")) {
      return createErrorResponse(http::status::bad_request,
                                 "Missing email or password");
    }

    std::string email = body["email"];
    std::string password = body["password"];

    auto result = auth_service_->loginEmailPwd(email, password);

    if (result) {
      auto [token, user] = result.value();
      nlohmann::json response_json = {
          {"success", true},
          {"message", "Login successful"},
          {"token", token},
          {"user",
           {{"id", user.id()},
            {"email", user.email()},
            {"username", user.username()},
            {"avatar", user.avatar()}}},
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::unauthorized, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(http::status::internal_server_error,
                               "Failed to process login: " +
                                   std::string(e.what()));
  }
}

http::response<http::string_body>
RestApiHandler::handleLoginEmailCode(const nlohmann::json &body) {
  try {
    if (!body.contains("email") || !body.contains("code")) {
      return createErrorResponse(http::status::bad_request,
                                 "Missing email or code");
    }

    std::string email = body["email"];
    std::string code = body["code"];

    auto result = auth_service_->loginEmailVeriCode(email, code);

    if (result) {
      auto [token, user] = result.value();
      nlohmann::json response_json = {
          {"success", true},
          {"message", "Login successful"},
          {"token", token},
          {"user",
           {{"id", user.id()},
            {"email", user.email()},
            {"username", user.username()},
            {"avatar", user.avatar()}}},
      };
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::unauthorized, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(http::status::internal_server_error,
                               "Failed to process login: " +
                                   std::string(e.what()));
  }
}

http::response<http::string_body>
RestApiHandler::handleValidateToken(const std::string &token) {
  try {
    auto result = auth_service_->validateToken(token);

    if (result) {
      nlohmann::json response_json = {{"success", true},
                                      {"message", "Token is valid"},
                                      {"user_id", result.value()}};
      return createJsonResponse(http::status::ok, response_json);
    } else {
      return createErrorResponse(http::status::unauthorized, result.error());
    }
  } catch (const std::exception &e) {
    return createErrorResponse(http::status::internal_server_error,
                               "Failed to validate token: " +
                                   std::string(e.what()));
  }
}

} // namespace user_service