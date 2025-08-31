#pragma once
#include <string>
#include <expected>

namespace video_service {
class StreamingService {
public:
  virtual ~StreamingService() = default;
  virtual void startServer(const std::string& video_dir) = 0;
  virtual void stopServer() = 0;
  virtual std::string getStreamUrl(const std::string& video_id) = 0;
};
}