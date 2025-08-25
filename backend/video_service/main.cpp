#include "domain/transcoder.hpp"
#include <print>
#include <future>

int main(int argc, char** argv) {
  video_service::Transcoder transcoder;

  video_service::VideoFormat constrant{
    .format = {"avi"},
    .codec={"libx264"}
  };
  auto fut1 = std::async(std::launch::async, std::bind(&video_service::Transcoder::transcode, "/home/ginger/Videos/308_part.mp4", "./test_vid1", constrant));
  auto fut2 = std::async(std::launch::async, std::bind(&video_service::Transcoder::transcode, "/home/ginger/Videos/blue_outpost_fps92.avi", "./test_vid2", constrant));

  const auto res1 = fut1.get();
  const auto res2 = fut2.get();
  if (!res1.has_value()){
    std::print("nooo!!!\n{}\n",res1.error());
  }
  if (!res2.has_value()){
    std::print("nooo!!!\n{}\n",res2.error());
  }
  return 0;
}