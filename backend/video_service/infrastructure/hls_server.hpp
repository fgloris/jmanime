#pragma once
#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "domain/streaming_service.hpp"

namespace video_service {
class HlsServer : public StreamingService {
public:
  HlsServer(const std::string& address, unsigned short port);
  
  void startServer(const std::string& video_dir) override;
  void stopServer() override;
  std::string getStreamUrl(const std::string& video_id) override;
  
private:
  class Session;
  void doAccept();
  void createPlaylist(const std::string& video_path);
  
  boost::asio::io_context ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::string video_dir_;
  bool running_;
  std::vector<std::shared_ptr<Session>> sessions_;
};
}