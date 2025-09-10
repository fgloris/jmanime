// subprocess_transcoder.cpp
#include "subprocess_transcoder.hpp"
#include "common/config.hpp"
#include <cstdio>

namespace video_service {

SubprocessTranscoder::SubprocessTranscoder() 
    : stop_(false), max_threads_(4), ffmpeg_available_(checkFFmpegInstalled()) {
  for (size_t i = 0; i < max_threads_; ++i) {
    workers_.emplace_back(std::async(std::launch::async, [this] {
      while (true) {
        std::packaged_task<std::expected<VideoFileStorage, std::string>()> task;
        
        {
          std::unique_lock<std::mutex> lock(queue_mutex_);
          condition_.wait(lock, [this] {
            return stop_ || !tasks_.empty();
          });
          
          if (stop_ && tasks_.empty()) return;
          
          task = std::move(tasks_.front());
          tasks_.pop();
        }
        
        task();
      }
    }));
  }
}

SubprocessTranscoder::~SubprocessTranscoder() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  
  condition_.notify_all();
  
  for (auto& worker : workers_) {
    if (worker.valid()) {
      worker.wait();
    }
  }
}

bool SubprocessTranscoder::checkFFmpegInstalled() {
  int result = std::system("ffmpeg -version > /dev/null 2>&1");
  return result == 0;
}

std::string SubprocessTranscoder::buildFFmpegCommand(const TranscoderParams& params) const {
  auto& config = config::Config::getInstance();
  
  std::string command = "ffmpeg -y -i \"" + params.input_path + "\" ";
  command += "-c:v libx264 -crf " + std::to_string(params.crf) + " ";
  command += "-preset " + params.preset + " ";
  command += "-profile:v " + params.profile + " ";
  
  if (params.copy_audio) {
    command += "-c:a copy ";
  } else {
    command += "-c:a aac ";
  }
  
  command += "-movflags +faststart ";
  command += "\"" + params.output_path + "\" ";
  command += "> /dev/null 2>&1";
  
  return command;
}

std::expected<VideoFileStorage, std::string> SubprocessTranscoder::executeTranscode(const TranscoderParams& params) {
  if (!ffmpeg_available_) {
    return std::unexpected("FFmpeg is not installed or not found in PATH");
  }
  
  std::string command = buildFFmpegCommand(params);
  int result = std::system(command.c_str());
  
  if (result != 0) {
    return std::unexpected("FFmpeg command failed with code: " + std::to_string(result));
  }
  
  VideoFileStorage storage;
  storage.path = params.output_path;
  
  VideoFormat format;
  format.format = "mp4";
  format.video_codec = "h264";
  format.audio_codec = "aac";
  format.crf = params.crf;
  
  storage.format = format;
  
  return storage;
}

std::expected<VideoFileStorage, std::string> SubprocessTranscoder::transcode(
    const std::string& input_path,
    const std::string& output_base_path) {
  
  TranscoderParams params;
  params.input_path = input_path;
  params.output_path = output_base_path + ".mp4";
  params.crf = 28;
  params.preset = "medium";
  params.profile = "baseline";
  params.copy_audio = true;
  
  return executeTranscode(params);
}

std::future<std::expected<VideoFileStorage, std::string>> SubprocessTranscoder::getTranscodeFuture(
    const std::string& input_path,
    const std::string& output_base_path,
    const VideoFormat& /* ignored_format */) {
  
  std::packaged_task<std::expected<VideoFileStorage, std::string>()> task(
    [this, input_path, output_base_path]() {
      return this->transcode(input_path, output_base_path);
    }
  );
  
  auto future = task.get_future();
  
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    tasks_.push(std::move(task));
  }
  
  condition_.notify_one();
  return future;
}

void SubprocessTranscoder::waitForAll() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  condition_.wait(lock, [this]() { return tasks_.empty(); });
}

} // namespace video_service