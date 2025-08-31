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

struct VideoMetaData {
  int duration{0};
  int64_t size{0};
  std::string title;
  std::string description;
  std::string url;
  std::string created_at;
  std::string updated_at;
};

struct VideoFile {
  std::string id;      // uuid, created when file first stored, should keep unchanged when file moves
  std::string path;    // file current path
  VideoFormat format;      // created with ffmpeg, changed when transcoded
  VideoMetaData metadata;  // created with ffmpeg
};

} // namespace video_service