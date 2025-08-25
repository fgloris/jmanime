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

};

struct VideoFile {
  std::string path;
  VideoFormat format;
  VideoMetaData metatdata;
};

} // namespace video_service