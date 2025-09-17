#pragma once

#include <cstddef>
#include <string>
#include <chrono>

namespace config {

struct ConnectionPoolConfig{
  size_t min_connections;
  size_t max_connections;
  std::chrono::milliseconds timeout;
  std::chrono::seconds idle_timeout;
};

struct DatabaseConfig {
  std::string host;
  unsigned int port;
  std::string user;
  std::string password;
  std::string db_name;
  std::string charset;
};

struct RedisConfig {
  std::string host;
  unsigned int port;
};

struct VideoStorageFormatConfig {
  std::string format;
  std::string codec_lib;
  std::string codec;
  int crf;
};


struct GrpcServiceConfig {
  std::string host;
  int port;
};

struct StreamingConfig {
  std::string host;
  int port;
};

struct AuthConfig {
  std::string jwt_secret;
  int jwt_expire_hours;
};

struct SMTPConfig {
  std::string server;
  int port;
  std::string password;
  std::string from_email;
  std::string email_sender_name;
  size_t queue_max_size;
};

class Config {
public:
static Config& getInstance() {
  static Config instance;
  return instance;
}

// Delete copy/move constructors and assign operators
Config(const Config&) = delete;
Config& operator=(const Config&) = delete;
Config(Config&&) = delete;
Config& operator=(Config&&) = delete;

// Getters
const DatabaseConfig& getDatabase() const { return database_; }
const RedisConfig& getRedis() const { return redis_ ;}
const GrpcServiceConfig& getUserService() const { return user_service_; }
const GrpcServiceConfig& getVideoService() const { return video_service_; }
const StreamingConfig& getStreaming() const { return streaming_; }
const AuthConfig& getAuth() const { return auth_; }
const VideoStorageFormatConfig& getFormat() const { return format_; }
const SMTPConfig& getSMTP() const { return smtp_; }
const ConnectionPoolConfig& getDBCntPool() const { return db_cp_; }
const ConnectionPoolConfig& getSMTPCntPool() const { return db_cp_; }
const std::string& getStoragePath() const { return storage_path_; }
std::string getUserServiceIpPort() const { return user_service_.host+":"+std::to_string(user_service_.port);}
std::string getVideoServiceIpPort() const { return video_service_.host+":"+std::to_string(video_service_.port);}

private:
  Config();

  RedisConfig redis_;
  DatabaseConfig database_;
  GrpcServiceConfig user_service_;
  GrpcServiceConfig video_service_;
  StreamingConfig streaming_;
  AuthConfig auth_;
  VideoStorageFormatConfig format_;
  SMTPConfig smtp_;
  ConnectionPoolConfig db_cp_;
  ConnectionPoolConfig smtp_cp_;
  std::string storage_path_;
};

} // namespace config
