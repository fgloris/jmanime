#include "hls_server.hpp"
#include <filesystem>
#include <fstream>
#include <boost/beast/http.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace video_service {
namespace http = boost::beast::http;
namespace fs = std::filesystem;

class HlsServer::Session : public std::enable_shared_from_this<Session> {
public:
  Session(boost::asio::ip::tcp::socket socket, const std::string& video_dir)
    : socket_(std::move(socket)), video_dir_(video_dir) {}
    
  void start() {
    doRead();
  }
  
private:
  void doRead() {
    req_ = {};
    http::async_read(socket_, buffer_, req_,
      [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
        if (!ec) {
          self->handleRequest();
        }
      });
  }
  
  void handleRequest() {
    std::string target{req_.target().data(), req_.target().size()};
    if (boost::algorithm::ends_with(target, ".m3u8")) {
      servePlaylist(target);
    } else if (boost::algorithm::ends_with(target, ".ts")) {
      serveSegment(target);
    } else {
      sendError(http::status::bad_request, "Invalid request");
    }
  }
  
  void servePlaylist(const std::string& target) {
    auto path = video_dir_ + target;
    if (!fs::exists(path)) {
      sendError(http::status::not_found, "Playlist not found");
      return;
    }
    
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
                        
    http::response<http::string_body> res{http::status::ok, req_.version()};
    res.set(http::field::content_type, "application/vnd.apple.mpegurl");
    res.body() = content;
    res.prepare_payload();
    
    http::async_write(socket_, res,
      [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
        if (!ec) {
          self->doRead();
        }
      });
  }
  
  void serveSegment(const std::string& target) {
    auto path = video_dir_ + target;
    if (!fs::exists(path)) {
      sendError(http::status::not_found, "Segment not found");
      return;
    }
    
    std::ifstream file(path, std::ios::binary);
    std::vector<char> content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
                              
    http::response<http::vector_body<char>> res{http::status::ok, req_.version()};
    res.set(http::field::content_type, "video/MP2T");
    res.body() = std::move(content);
    res.prepare_payload();
    
    http::async_write(socket_, res,
      [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
        if (!ec) {
          self->doRead();
        }
      });
  }
  
  void sendError(http::status status, const std::string& error) {
    http::response<http::string_body> res{status, req_.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = error;
    res.prepare_payload();
    
    http::async_write(socket_, res,
      [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
        if (!ec) {
          self->doRead();
        }
      });
  }
  
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  std::string video_dir_;
};

HlsServer::HlsServer(const std::string& address, unsigned short port)
  : acceptor_(ioc_), running_(false) {
  boost::asio::ip::tcp::endpoint endpoint(
    boost::asio::ip::make_address(address), port);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

void HlsServer::startServer(const std::string& video_dir) {
  video_dir_ = video_dir;
  running_ = true;
  doAccept();
  ioc_.run();
}

void HlsServer::stopServer() {
  running_ = false;
  ioc_.stop();
}

std::string HlsServer::getStreamUrl(const std::string& video_id) {
  return "http://localhost:8080/" + video_id + ".m3u8";
}

void HlsServer::doAccept() {
  acceptor_.async_accept(
    [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
      if (!ec && running_) {
        auto session = std::make_shared<Session>(std::move(socket), video_dir_);
        sessions_.push_back(session);
        session->start();
      }
      
      if (running_) {
        doAccept();
      }
    });
}

void HlsServer::createPlaylist(const std::string& video_path) {
  auto basename = fs::path(video_path).stem().string();
  auto playlist_path = video_dir_ + "/" + basename + ".m3u8";
  
  std::ofstream playlist(playlist_path);
  playlist << "#EXTM3U\n"
          << "#EXT-X-VERSION:3\n"
          << "#EXT-X-TARGETDURATION:10\n"
          << "#EXT-X-MEDIA-SEQUENCE:0\n";
          
  for (const auto& entry : fs::directory_iterator(video_dir_)) {
    if (entry.path().extension() == ".ts" && 
        entry.path().stem().string().starts_with(basename)) {
      playlist << "#EXTINF:10.0,\n"
              << entry.path().filename().string() << "\n";
    }
  }
  
  playlist << "#EXT-X-ENDLIST\n";
}
}
