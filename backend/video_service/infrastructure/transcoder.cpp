#include "transcoder.hpp"

namespace video_service {
Transcoder::Transcoder(int loglevel) {
  av_log_set_level(loglevel);
}

void Transcoder::setLogLevel(int loglevel) {
  av_log_set_level(loglevel);
}

std::expected<VideoFileStorage, std::string> Transcoder::transcode(const std::string& input_path,
                                                                  const std::string& output_path,
                                                                  const VideoFormat& target_format) {
  Context ctx;
  if (const auto result = openInputFile(input_path, ctx); !result.has_value()) {
    return std::unexpected<std::string>(result.error());
  }

  if (const auto result = openOutputFile(output_path, target_format, ctx); !result.has_value()) {
    return std::unexpected<std::string>(result.error());
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

  freeContext(ctx);
  return VideoFileStorage{
    output_path,
    target_format
  };
}

std::expected<bool, std::string> Transcoder::openInputFile(const std::string& input_path, Transcoder::Context& ctx) {
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
  return true;
}

std::expected<bool, std::string> Transcoder::openOutputFile(const std::string& output_path, const VideoFormat& format, Transcoder::Context& ctx) {
  int ret = avformat_alloc_output_context2(&ctx.output_ctx, nullptr, 
                                          format.format.c_str(), output_path.c_str());
  if (!ctx.output_ctx) {
    return std::unexpected<std::string>("Could not create output context");
  }

  AVStream* out_stream = avformat_new_stream(ctx.output_ctx, nullptr);
  if (!out_stream) {
    return std::unexpected<std::string>("Failed to allocate output stream");
  }

  if (const auto result = initVideoEncoder(format, ctx); !result.has_value()) {
    return std::unexpected<std::string>(result.error());
  }

  if (avcodec_parameters_from_context(out_stream->codecpar, ctx.video_enc_ctx) < 0) {
    return std::unexpected<std::string>("Failed to copy encoder parameters to output stream");
  }

  if (!(ctx.output_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&ctx.output_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
      return std::unexpected<std::string>("Could not open output file");
    }
  }
  return true;
}


std::expected<bool, std::string> Transcoder::initVideoDecoder(Context& ctx) {
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
  return true;
}

std::expected<bool, std::string> Transcoder::initVideoEncoder(const VideoFormat& format, Context& ctx) {
  const AVCodec* encoder = avcodec_find_encoder_by_name(format.codec.c_str());
  if (!encoder) {
    return std::unexpected<std::string>("Failed to find encoder: " + format.codec);
  }

  ctx.video_enc_ctx = avcodec_alloc_context3(encoder);
  if (!ctx.video_enc_ctx) {
    return std::unexpected<std::string>("Failed to allocate encoder context");
  }

  // Set basic parameters
  ctx.video_enc_ctx->height = format.height > 0 ? format.height : ctx.video_dec_ctx->height;
  ctx.video_enc_ctx->width = format.width > 0 ? format.width : ctx.video_dec_ctx->width;
  ctx.video_enc_ctx->sample_aspect_ratio = ctx.video_dec_ctx->sample_aspect_ratio;
  ctx.video_enc_ctx->bit_rate = format.bitrate > 0 ? format.bitrate : ctx.video_dec_ctx->bit_rate;

  // Set time_base and framerate
  AVRational input_framerate = ctx.video_dec_ctx->framerate;
  if (input_framerate.num == 0 || input_framerate.den == 0) {
    input_framerate = av_guess_frame_rate(ctx.input_ctx, ctx.video_stream, nullptr);
  }
  ctx.video_enc_ctx->time_base = av_inv_q(input_framerate);
  ctx.video_enc_ctx->framerate = input_framerate;

  // Set pixel format
  // For H.264, typically use yuv420p
  if (format.codec == "libx264") {
    ctx.video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  } else {
      // Try to use same format as input
    ctx.video_enc_ctx->pix_fmt = ctx.video_dec_ctx->pix_fmt;
  }
  
  // Set GOP (keyframe interval) size
  if (format.format == "mp4") {
    ctx.video_enc_ctx->gop_size = 12;
    ctx.video_enc_ctx->max_b_frames = 2;
  }

  // Set profile for H.264
  if (format.codec == "libx264") {
    // 'medium' preset for good quality/speed trade-off
    av_opt_set(ctx.video_enc_ctx->priv_data, "preset", "medium", 0);
    // 'high' profile for better quality
    av_opt_set(ctx.video_enc_ctx->priv_data, "profile", "high", 0);
  }

  // Some formats require this flag
  if (ctx.output_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
    ctx.video_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  // Open encoder
  int ret = avcodec_open2(ctx.video_enc_ctx, encoder, nullptr);
  if (ret < 0) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    return std::unexpected<std::string>(std::string("Failed to open encoder: ") + err_buf);
  }

  return true;
}

void Transcoder::freeContext(Context& ctx) {
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