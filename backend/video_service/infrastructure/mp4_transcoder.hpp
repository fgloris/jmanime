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
// 专门用于高质量MP4+H265编码的转码器
class MP4Transcoder: public TranscodingService {
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
  MP4Transcoder(int loglevel = AV_LOG_ERROR);

  std::expected<VideoFileStorage, std::string> transcode(
    const std::string& input_path,
    const std::string& output_base_path
  ); 
    
  std::future<std::expected<VideoFileStorage, std::string>> getTranscodeFuture(
    const std::string& input_path,
    const std::string& output_base_path,
    const VideoFormat& /* ignored_format */
  ) override {
    return std::async(std::launch::async, 
      std::bind(&video_service::MP4Transcoder::transcode, this, input_path, output_base_path));
  }
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
  static std::expected<VideoFormat, std::string> openOutputFile(const std::string& output_path, Context& ctx);
  static std::expected<bool, std::string> initVideoDecoder(Context& ctx);
  static std::expected<VideoFormat, std::string> initVideoEncoder(Context& ctx);
  static void freeContext(Context& ctx);
};

} // namespace video_service