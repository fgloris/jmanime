#include "infrastructure/mp4_transcoder.hpp"
#include <iostream>
#include <filesystem>

int main() {
  video_service::MP4Transcoder transcoder;
  
  std::string input_path = "/home/ginger/Videos/jmanime/far.avi";
  std::string output_path = "/home/ginger/Videos/jmanime/test";

  if (!std::filesystem::exists(input_path)) {
    std::cerr << "Input file does not exist: " << input_path << std::endl;
    return 1;
  }

  std::cout << "Starting transcoding..." << std::endl;
  std::cout << "Input: " << input_path << std::endl;
  std::cout << "Output: " << output_path << ".mp4" << std::endl;

  // 执行转码
  auto result = transcoder.transcode(input_path, output_path);
  
  if (result) {
    std::cout << "Transcoding successful!" << std::endl;
    std::cout << result->debug() << std::endl;
  } else {
    std::cerr << "Transcoding failed: " << result.error() << std::endl;
    return 1;
  }

  return 0;
}
