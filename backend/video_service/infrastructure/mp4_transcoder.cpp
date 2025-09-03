#include "mp4_transcoder.hpp"
#include "domain/video.hpp"
#include "common/config.hpp"
#include <cstddef>
#include <cstring>
#include <expected>
#include <libavutil/log.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace video_service {

bool is_pix_fmt_supported(const AVCodec* encoder, enum AVPixelFormat pix_fmt) {
  if (!encoder || !encoder->pix_fmts) return false;
  const enum AVPixelFormat* p = encoder->pix_fmts;
  while (*p != AV_PIX_FMT_NONE) {
    if (*p == pix_fmt) return true;
    else p++;
  }
  return false;
}

bool contains_format(std::string format_list, std::string format) {
  std::transform(format_list.begin(), format_list.end(), format_list.begin(), 
    [](unsigned char c) { return std::tolower(c); });
  std::transform(format.begin(), format.end(), format.begin(), 
      [](unsigned char c) { return std::tolower(c); });
  std::vector<std::string> formats;
  std::stringstream ss(format_list);
  std::string item;

  while (std::getline(ss, item, ',')) {
      formats.push_back(item);
  }
  for (const auto& f: formats){
    if (!std::strcmp(f.c_str(), format.c_str())){
      return true;
    }
  }
  return false;
}

MP4Transcoder::MP4Transcoder(int loglevel) {
  av_log_set_level(AV_LOG_ERROR);
}

void MP4Transcoder::setLogLevel(int loglevel) {
  av_log_set_level(loglevel);
}

std::expected<VideoFileStorage, std::string> MP4Transcoder::transcode(const std::string& input_path, 
                                                                     const std::string& output_base_path) {
  Context ctx;
  std::string output_path = output_base_path + ".mp4";
  auto& config = config::Config::getInstance();
  
  if (const auto ret = openInputFile(input_path, ctx); !ret.has_value()) {
    return std::unexpected<std::string>(ret.error());
  }else if ((!std::strcmp(ctx.video_dec_ctx->codec->name, config.getFormat().codec.c_str()))
    && contains_format(ctx.input_ctx->iformat->name, "mp4")){

    avcodec_free_context(&ctx.video_enc_ctx);
    avformat_close_input(&ctx.input_ctx);
    try {
      std::filesystem::copy_file(input_path, output_path);
    } catch (const std::filesystem::filesystem_error& e) {
      if (e.code() == std::errc::file_exists) {
        try {
            std::error_code ec;
            if (std::filesystem::remove(output_path, ec)) {
                std::filesystem::copy_file(input_path, output_path);
            } else {
                return std::unexpected<std::string>(
                  "Failed to remove existing file: " + output_base_path + 
                  ", Error: " + ec.message()
                );
            }
        } catch (const std::filesystem::filesystem_error& remove_error) {
            return std::unexpected<std::string>(
                std::string("Error handling existing file: ") + remove_error.what()
            );
        }
      }else return std::unexpected<std::string>(std::string("Error copying file:") + e.what());
    }
    return VideoFileStorage{
      .path = output_path,
      .format = getVideoFormatFromContext(ctx, "dec")
    };
  }

  auto f = getVideoFormatFromContext(ctx, "dec");
  std::cout<<f.debug()<<std::endl;

  if (const auto ret = openOutputFile(output_path, ctx, config.getFormat().codec_lib, config.getFormat().codec, 23);
      !ret.has_value()) {
    return std::unexpected<std::string>(ret.error());
  }

  if (avformat_write_header(ctx.output_ctx, nullptr) < 0) {
    return std::unexpected<std::string>("Failed to write header");
  }

  int64_t total_frames = ctx.video_stream->nb_frames;
  int64_t processed_frames = 0;

  while (true) {
    if (av_read_frame(ctx.input_ctx, ctx.packet) < 0) {
      break;
    }

    if (ctx.packet->stream_index == ctx.video_stream_idx) {
      // Send packet to decoder
      if (avcodec_send_packet(ctx.video_dec_ctx, ctx.packet) < 0) {
        av_packet_unref(ctx.packet);
        return std::unexpected<std::string>("Error sending packet to decoder");
      }

      while (true) {
        
        if (int ret = avcodec_receive_frame(ctx.video_dec_ctx, ctx.frame); 
            ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        } else if (ret < 0) {
          return std::unexpected<std::string>("Error receiving frame from decoder");
        }
        
        if (avcodec_send_frame(ctx.video_enc_ctx, ctx.frame) < 0) {
          return std::unexpected<std::string>("Error sending frame to encoder");
        }

        while (true) {
          if (int ret = avcodec_receive_packet(ctx.video_enc_ctx, ctx.packet);
              ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
          } else if (ret < 0) {
            return std::unexpected<std::string>("Error receiving packet from encoder");
          }

          // Write encoded packet
          if (av_interleaved_write_frame(ctx.output_ctx, ctx.packet) < 0) {
            return std::unexpected<std::string>("Error writing output packet");
          }
        }

        processed_frames++;
      }
    }
    av_packet_unref(ctx.packet);
  }

  // Flush encoders
  avcodec_send_frame(ctx.video_enc_ctx, nullptr);
  while (true) {
    
    if (int ret = avcodec_receive_packet(ctx.video_enc_ctx, ctx.packet); 
        ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }else if (ret < 0) {
      return std::unexpected<std::string>("Error flushing encoder");
    }
    if (av_interleaved_write_frame(ctx.output_ctx, ctx.packet) < 0) {
      return std::unexpected<std::string>("Error writing output packet during flush");
    }
  }

  // Write trailer
  if (av_write_trailer(ctx.output_ctx) < 0) {
    return std::unexpected<std::string>("Failed to write trailer");
  }

  VideoFormat fmt = getVideoFormatFromContext(ctx, "enc");
  fmt.crf = 23;

  freeContext(ctx);
  return VideoFileStorage{
    .path = output_path,
    .format = fmt
  };
}

std::expected<void, std::string> MP4Transcoder::openInputFile(const std::string& input_path, MP4Transcoder::Context& ctx) {
  if (avformat_open_input(&(ctx.input_ctx), input_path.c_str(), nullptr, nullptr) < 0) {
    return std::unexpected<std::string>("Could not open input file");
  }
  if (avformat_find_stream_info(ctx.input_ctx, nullptr) < 0) {
    return std::unexpected<std::string>("Could not find stream info");
  }
  if (ctx.video_stream_idx = av_find_best_stream(ctx.input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    ctx.video_stream_idx < 0) {
    return std::unexpected<std::string>("Could not find video stream");
  }
  ctx.video_stream = ctx.input_ctx->streams[ctx.video_stream_idx];

  
  if (const auto result = initVideoDecoder(ctx); !result.has_value()) {
    return std::unexpected<std::string>(result.error());
  }

  ctx.frame = av_frame_alloc();
  ctx.packet = av_packet_alloc();
  if (!ctx.frame || !ctx.packet) {
    return std::unexpected<std::string>("Could not allocate frame or packet");
  }
  return {};
}

std::expected<void, std::string> MP4Transcoder::openOutputFile(const std::string& output_path, 
                                                                     MP4Transcoder::Context& ctx, 
                                                                     const std::string& codec_lib, 
                                                                     const std::string& target_codec,
                                                                     int crf
                                                                    ) {
  int ret = avformat_alloc_output_context2(&ctx.output_ctx, nullptr, 
                                          "mp4", output_path.c_str());
  if (!ctx.output_ctx) {
    return std::unexpected<std::string>("Could not create output context");
  }

  AVStream* out_stream = avformat_new_stream(ctx.output_ctx, nullptr);
  if (!out_stream) {
    return std::unexpected<std::string>("Failed to allocate output stream");
  }

  
  if (const auto ret = initVideoEncoder(ctx, codec_lib, target_codec, crf); !ret.has_value()) {
    return std::unexpected<std::string>(ret.error());
  }

  if (avcodec_parameters_from_context(out_stream->codecpar, ctx.video_enc_ctx) < 0) {
    return std::unexpected<std::string>("Failed to copy encoder parameters to output stream");
  }

  out_stream->time_base = ctx.video_enc_ctx->time_base;

  if (!(ctx.output_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&ctx.output_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
      return std::unexpected<std::string>("Could not open output file");
    }
  }
  return {};
}


std::expected<void, std::string> MP4Transcoder::initVideoDecoder(Context& ctx) {
  const AVCodec* decoder = avcodec_find_decoder(ctx.video_stream->codecpar->codec_id);
  if (!decoder) {
    return std::unexpected<std::string>("Failed to find decoder");
  }
  if (ctx.video_dec_ctx = avcodec_alloc_context3(decoder); !ctx.video_dec_ctx) {
    return std::unexpected<std::string>("Failed to allocate decoder context");
  }
  if (avcodec_parameters_to_context(ctx.video_dec_ctx, ctx.video_stream->codecpar) < 0) {
    return std::unexpected<std::string>("Failed to copy decoder params");
  }
  if (avcodec_open2(ctx.video_dec_ctx, decoder, nullptr) < 0) {
    return std::unexpected<std::string>("Failed to open decoder");
  }
  return {};
}

std::expected<void, std::string> MP4Transcoder::initVideoEncoder(Context& ctx, const std::string& codec_lib, const std::string& target_codec, int crf) {
  const AVCodec* encoder = avcodec_find_encoder_by_name(codec_lib.c_str());
  if (!encoder) {
    return std::unexpected<std::string>("Failed to find encoder: " + codec_lib);
  }

  ctx.video_enc_ctx = avcodec_alloc_context3(encoder);
  if (!ctx.video_enc_ctx) {
    return std::unexpected<std::string>("Failed to allocate encoder context");
  }

  ctx.video_enc_ctx->height = ctx.video_dec_ctx->height;
  ctx.video_enc_ctx->width = ctx.video_dec_ctx->width;
  ctx.video_enc_ctx->sample_aspect_ratio = ctx.video_dec_ctx->sample_aspect_ratio;
  
  av_opt_set_int(ctx.video_enc_ctx->priv_data, "crf", crf, 0);

  AVRational input_framerate = av_guess_frame_rate(ctx.input_ctx, ctx.video_stream, NULL);
  if (input_framerate.num == 0 || input_framerate.den == 0) {
    input_framerate = av_make_q(30, 1);
  }
  ctx.video_enc_ctx->time_base = av_inv_q(input_framerate);
  ctx.video_enc_ctx->framerate = input_framerate;
  
  if (is_pix_fmt_supported(encoder, ctx.video_dec_ctx->pix_fmt)){
    ctx.video_enc_ctx->pix_fmt = ctx.video_dec_ctx->pix_fmt;
  }else{
    ctx.video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  }

  int ret = avcodec_open2(ctx.video_enc_ctx, encoder, nullptr);
  if (ret < 0) {
    char error[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, error, AV_ERROR_MAX_STRING_SIZE);
    return std::unexpected(std::string("Failed to open encoder: ") + error);
  }
  return {};
}

VideoFormat MP4Transcoder::getVideoFormatFromContext(const MP4Transcoder::Context& ctx, const std::string& enc_or_dec){
  if ((!std::strcmp(enc_or_dec.c_str(), "enc"))
      && ctx.video_enc_ctx != nullptr){
    return VideoFormat{
      .width = ctx.video_enc_ctx->width,
      .height = ctx.video_enc_ctx->height,
      .bitrate = ctx.video_enc_ctx->bit_rate,
      .pix_fmt = ctx.video_enc_ctx->pix_fmt,
      .format = "mp4",
      .codec = ctx.video_enc_ctx->codec->name
    };
  }else if ((!std::strcmp(enc_or_dec.c_str(), "dec"))
      && ctx.video_dec_ctx != nullptr){
    return VideoFormat{
      .width = ctx.video_dec_ctx->width,
      .height = ctx.video_dec_ctx->height,
      .bitrate = ctx.video_dec_ctx->bit_rate,
      .pix_fmt = ctx.video_dec_ctx->pix_fmt,
      .format = "mp4",
      .codec = ctx.video_dec_ctx->codec->name
    };
  }
  return VideoFormat{};
}

void MP4Transcoder::freeContext(Context& ctx) {
  avcodec_free_context(&ctx.video_dec_ctx);
  avcodec_free_context(&ctx.video_enc_ctx);
  avformat_close_input(&ctx.input_ctx);
  if (ctx.output_ctx && !(ctx.output_ctx->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&ctx.output_ctx->pb);
  }
  avformat_free_context(ctx.output_ctx);
  av_frame_free(&ctx.frame);
  av_packet_free(&ctx.packet);
}

} // namespace video_service