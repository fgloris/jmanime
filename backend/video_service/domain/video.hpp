#pragma once
#include <string>

namespace video_service {

struct VideoFormat {
  int width{0};
  int height{0};
  int bitrate{0};      // in bits per second
  std::string format;  // container format like "mp4", "mkv"
  std::string codec;   // video codec like "h264", "h265", "libx264"
};

struct VideoFileStorage {
  std::string path;        // file current path
  VideoFormat format;      // created with ffmpeg, changed when transcoded
};

struct VideoPresentInfo {
  int duration{0};
  std::string title;
  std::string description;
  std::string url;
  std::string created_at;
  std::string updated_at;
};

struct VideoFile {
  std::string uuid; 
  VideoFileStorage storage;  // created with ffmpeg, changed when transcoded
  VideoPresentInfo info;    // created with ffmpeg
};

#define RAW_DOWNLOAD_FILE_SUFFIX "_raw"
#define TEMP_FILE_SUFFIX "_temp"

} // namespace video_service