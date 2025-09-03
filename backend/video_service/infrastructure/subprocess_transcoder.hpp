// subprocess_transcoder.hpp
#pragma once

#include "domain/transcoding_service.hpp"
#include "domain/video.hpp"

#include <future>
#include <string>
#include <expected>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

namespace video_service {

class SubprocessTranscoder : public TranscodingService {
public:
  SubprocessTranscoder();
  ~SubprocessTranscoder();

  std::expected<VideoFileStorage, std::string> transcode(
    const std::string& input_path,
    const std::string& output_base_path
  ) override; 
    
  std::future<std::expected<VideoFileStorage, std::string>> getTranscodeFuture(
    const std::string& input_path,
    const std::string& output_base_path,
    const VideoFormat& /* ignored_format */
  ) override;

  void waitForAll() override;

private:
  struct TranscoderParams {
    std::string input_path;
    std::string output_path;
    int crf;
    std::string preset;
    std::string profile;
    bool copy_audio;
  };

  std::expected<VideoFileStorage, std::string> executeTranscode(const TranscoderParams& params);
  bool checkFFmpegInstalled();
  std::string buildFFmpegCommand(const TranscoderParams& params) const;

  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::queue<std::packaged_task<std::expected<VideoFileStorage, std::string>()>> tasks_;
  std::vector<std::future<void>> workers_;
  bool stop_;
  const size_t max_threads_;
  bool ffmpeg_available_;
};

} // namespace video_service