#include "mysql_video_repository.hpp"
#include <format>

namespace video_service {
MysqlVideoRepository::MysqlVideoRepository(const std::string& host,
                                         const std::string& user,
                                         const std::string& password,
                                         const std::string& database)
  : conn_(mysql_init(nullptr), mysql_close) {
  if (!mysql_real_connect(conn_.get(), host.c_str(), user.c_str(),
                         password.c_str(), database.c_str(), 0, nullptr, 0)) {
    throw std::runtime_error(mysql_error(conn_.get()));
  }
}

MysqlVideoRepository::~MysqlVideoRepository() = default;

std::expected<VideoFile, std::string> MysqlVideoRepository::save(const VideoFile& video) {
  auto query = std::format("INSERT INTO videos (id, path, format, metadata) "
                          "VALUES ('{}', '{}', '{}', '{}') "
                          "ON DUPLICATE KEY UPDATE "
                          "path = VALUES(path), format = VALUES(format), "
                          "metadata = VALUES(metadata)",
                          video.id, video.path,
                          video.format.format, video.metadata.title);
                          
  if (mysql_query(conn_.get(), query.c_str())) {
    return std::unexpected(mysql_error(conn_.get()));
  }
  return video;
}

std::expected<VideoFile, std::string> MysqlVideoRepository::findById(const std::string& id) {
  auto query = std::format("SELECT * FROM videos WHERE id = '{}'", id);
  
  if (mysql_query(conn_.get(), query.c_str())) {
    return std::unexpected(mysql_error(conn_.get()));
  }
  
  auto result = mysql_store_result(conn_.get());
  if (!result) {
    return std::unexpected("No result set");
  }
  
  auto row = mysql_fetch_row(result);
  if (!row) {
    mysql_free_result(result);
    return std::unexpected("Video not found");
  }
  
  VideoFile video;
  video.id = row[0];
  video.path = row[1];
  video.format.format = row[2];
  video.metadata.title = row[3];
  
  mysql_free_result(result);
  return video;
}

std::expected<std::vector<VideoFile>, std::string> MysqlVideoRepository::findAll() {
  if (mysql_query(conn_.get(), "SELECT * FROM videos")) {
    return std::unexpected(mysql_error(conn_.get()));
  }
  
  auto result = mysql_store_result(conn_.get());
  if (!result) {
    return std::unexpected("No result set");
  }
  
  std::vector<VideoFile> videos;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    VideoFile video;
    video.id = row[0];
    video.path = row[1];
    video.format.format = row[2];
    video.metadata.title = row[3];
    videos.push_back(video);
  }
  
  mysql_free_result(result);
  return videos;
}

std::expected<bool, std::string> MysqlVideoRepository::remove(const std::string& id) {
  auto query = std::format("DELETE FROM videos WHERE id = '{}'", id);
  
  if (mysql_query(conn_.get(), query.c_str())) {
    return std::unexpected(mysql_error(conn_.get()));
  }
  return true;
}
}
