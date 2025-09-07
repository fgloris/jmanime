#include "config.hpp"
#include <chrono>

namespace config {
  
  Config::Config() {
    database_ = {
      .host = "localhost",
      .user = "root",
      .password = "114472988",
      .db_name = "jmanime_db",
      .charset = "utf8mb4",
      .min_connections = 5,
      .max_connections = 20,
      .timeout = std::chrono::milliseconds(5000),
      .idle_timeout = std::chrono::seconds(600)
    };

    user_service_ = {
      "0.0.0.0", 
      50051
    };

    video_service_ = {
      .host = "0.0.0.0",
      .port = 50052
    };

    streaming_ = {
      .host = "0.0.0.0",
      .port = 8080
    };

    auth_ = {
      .jwt_secret = "your-secret-key",
      .jwt_expire_hours = 24
    };

    format_ = {
      .format = "mp4",
      .codec_lib = "libx264",
      .codec = "h264",
      .crf = 28
    };

    smtp_ = {
      .server = "smtp.163.com",
      .port = 465,
      .password = "PDV3DvKnMmaYUA5p",
      .from_email = "19313199238@163.com",
    };

    storage_path_ = "/home/ginger/Videos/jmanime";
  }
}