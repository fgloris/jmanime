#pragma once

// project
#include "domain/transcoding_service.hpp"

// std
#include <future>
#include <string>
#include <expected>
#include <functional>

// ffmpeg
extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavfilter/buffersink.h>
  #include <libavfilter/buffersrc.h>
  #include <libavutil/channel_layout.h>
  #include <libavutil/log.h>
  #include <libavutil/mem.h>
  #include <libavutil/opt.h>
  #include <libavutil/pixdesc.h>
}

namespace video_service {
class Transcoder: public TranscodingService {
  public:
  // AV_LOG_QUIET   = -8
  // AV_LOG_PANIC   =  0
  // AV_LOG_FATAL   =  8
  // AV_LOG_ERROR   = 16
  // AV_LOG_WARNING = 24
  // AV_LOG_INFO    = 32
  // AV_LOG_VERBOSE = 40
  // AV_LOG_DEBUG   = 48
  // AV_LOG_TRACE   = 56
  Transcoder(int loglevel = AV_LOG_ERROR);
  static std::expected<VideoFile, std::string> transcode(const std::string& input_path,
                                                        const std::string& output_path,
                                                        const VideoFormat& constrant);
  std::future<std::expected<VideoFile, std::string>>getTranscodeFuture(const std::string& input_path,
                                                                      const std::string& output_path,
                                                                      const VideoFormat& constrant) override {
    return std::async(std::launch::async, std::bind(&video_service::Transcoder::transcode, input_path, output_path, constrant));}
  void setLogLevel(int loglevel);
  struct Context {
    AVFormatContext* input_ctx{nullptr};
    AVFormatContext* output_ctx{nullptr};
    AVCodecContext* video_dec_ctx{nullptr};
    AVCodecContext* video_enc_ctx{nullptr};
    AVStream* video_stream{nullptr};
    AVFrame* frame{nullptr};
    AVPacket* packet{nullptr};
    int video_stream_idx{-1};
  };
private:
  static std::expected<bool, std::string> openInputFile(const std::string& output_path, Context& ctx);
  static std::expected<bool, std::string> openOutputFile(const std::string& output_path, const VideoFormat& format, Context& ctx);
  static std::expected<bool, std::string> initVideoDecoder(Context& ctx);
  static std::expected<bool, std::string> initVideoEncoder(const VideoFormat& format, Context& ctx);
  static void freeContext(Context& ctx);
};

} // namespace video_service