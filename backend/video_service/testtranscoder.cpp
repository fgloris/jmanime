#include "infrastructure/mp4_transcoder.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>

int main() {
  video_service::MP4Transcoder transcoder;
  
  std::string input_path = "/home/ginger/Videos/jmanime/正在播放《瑞克和莫蒂第八季》第01集_高清HD全集在线观看_樱花动漫";
  std::string output_path = "/home/ginger/Videos/jmanime/test";

  if (!std::filesystem::exists(input_path)) {
    std::cerr << "Input file does not exist: " << input_path << std::endl;
    return 1;
  }

  std::cout << "Starting transcoding..." << std::endl;
  std::cout << "Input: " << input_path << std::endl;
  std::cout << "Output: " << output_path << ".mp4" << std::endl;

  auto start_time = std::chrono::high_resolution_clock::now();

  auto result = transcoder.transcode(input_path, output_path);
  
  auto end_time = std::chrono::high_resolution_clock::now();
  
  // 计算执行时间
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  if (result) {
    std::cout << "Transcoding successful!" << std::endl;
    std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    std::cout << result->debug() << std::endl;
  } else {
    std::cerr << "Transcoding failed: " << result.error() << std::endl;
    std::cerr << "Time taken: " << duration.count() << " milliseconds (before failure)" << std::endl;
    return 1;
  }

  return 0;
}
