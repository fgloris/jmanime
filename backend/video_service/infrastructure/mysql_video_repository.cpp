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
  auto query = std::format(
    "INSERT INTO videos ("
    "  uuid, storage_path, format_width, format_height, format_bitrate,"
    "  format_container, format_codec, info_duration, info_title,"
    "  info_description, info_url, info_created_at, info_updated_at"
    ") VALUES ("
    "  '{}', '{}', {}, {}, {},"
    "  '{}', '{}', {}, '{}',"
    "  '{}', '{}', '{}', '{}'"
    ") ON DUPLICATE KEY UPDATE "
    "  storage_path = VALUES(storage_path),"
    "  format_width = VALUES(format_width),"
    "  format_height = VALUES(format_height),"
    "  format_bitrate = VALUES(format_bitrate),"
    "  format_container = VALUES(format_container),"
    "  format_codec = VALUES(format_codec),"
    "  info_duration = VALUES(info_duration),"
    "  info_title = VALUES(info_title),"
    "  info_description = VALUES(info_description),"
    "  info_url = VALUES(info_url),"
    "  info_updated_at = VALUES(info_updated_at)",
    video.uuid,
    video.storage.path,
    video.storage.format.width,
    video.storage.format.height,
    video.storage.format.bitrate,
    video.storage.format.format,
    video.storage.format.video_codec,
    video.info.duration,
    video.info.title,
    video.info.description,
    video.info.url,
    video.info.created_at,
    video.info.updated_at);
                          
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
  video.uuid = row[0];
  video.storage.path = row[1];
  video.storage.format.width = std::stoi(row[2]);
  video.storage.format.height = std::stoi(row[3]);
  video.storage.format.bitrate = std::stoi(row[4]);
  video.storage.format.format = row[5];
  video.storage.format.video_codec = row[6];
  video.info.duration = std::stoi(row[7]);
  video.info.title = row[8];
  video.info.description = row[9];
  video.info.url = row[10];
  video.info.created_at = row[11];
  video.info.updated_at = row[12];
  
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
    video.uuid = row[0];
    video.storage.path = row[1];
    video.storage.format.format = row[2];
    video.info.title = row[3];
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
