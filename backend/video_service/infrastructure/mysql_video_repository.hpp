#pragma once
#include "domain/video_respository.hpp"
#include <mysql/mysql.h>
#include <memory>

namespace video_service {
class MysqlVideoRepository : public VideoRepository {
public:
  MysqlVideoRepository(const std::string& host, 
                      const std::string& user,
                      const std::string& password,
                      const std::string& database);
  ~MysqlVideoRepository();
  
  std::expected<VideoFile, std::string> save(const VideoFile& video) override;
  std::expected<VideoFile, std::string> findById(const std::string& id) override;
  std::expected<std::vector<VideoFile>, std::string> findAll() override;
  std::expected<bool, std::string> remove(const std::string& id) override;

private:
  std::unique_ptr<MYSQL, decltype(&mysql_close)> conn_;
};
}