#pragma once
#include <string>
#include <format>
extern "C" {
  #include <libavutil/pixfmt.h>
  #include <libavutil/pixdesc.h>
}

namespace video_service {

struct VideoFormat {
  int width{0};
  int height{0};
  int crf{23};
  long bitrate{0};
  enum AVPixelFormat pix_fmt;
  std::string format;  // container format like "mp4", "mkv"
  std::string codec;   // video codec like "h264", "h265", "libx264"
  std::string debug(){
    return std::format("format:{},codec:{},width:{},height:{},resolution:{},crf:{},bitrate:{},pix_fmt:{}",
      format,codec,width,height,width*height,crf,bitrate,av_get_pix_fmt_name(pix_fmt));
  }
};

/*
[STREAM]
index=0
codec_name=hevc
codec_long_name=H.265 / HEVC (High Efficiency Video Coding)
profile=Main
codec_type=video
codec_tag_string=hev1
codec_tag=0x31766568
width=1280
height=720
coded_width=1280
coded_height=720
closed_captions=0
has_b_frames=2
sample_aspect_ratio=1:1
display_aspect_ratio=16:9
pix_fmt=yuv420p
level=93
color_range=tv
color_space=unknown
color_transfer=unknown
color_primaries=unknown
chroma_location=left
field_order=unknown
refs=1
id=N/A
r_frame_rate=5625/32
avg_frame_rate=57375/326
time_base=1/90000
start_pts=0
start_time=0.000000
duration_ts=417280
duration=4.636444
bit_rate=5302244
max_bit_rate=N/A
bits_per_raw_sample=N/A
nb_frames=816
nb_read_frames=N/A
nb_read_packets=N/A
DISPOSITION:default=1
DISPOSITION:dub=0
DISPOSITION:original=0
DISPOSITION:comment=0
DISPOSITION:lyrics=0
DISPOSITION:karaoke=0
DISPOSITION:forced=0
DISPOSITION:hearing_impaired=0
DISPOSITION:visual_impaired=0
DISPOSITION:clean_effects=0
DISPOSITION:attached_pic=0
DISPOSITION:timed_thumbnails=0
TAG:language=und
TAG:handler_name=VideoHandler
TAG:vendor_id=[0][0][0][0]
[/STREAM]

*/

struct VideoFileStorage {
  std::string path;        // file current path
  VideoFormat format;      // created with ffmpeg, changed when transcoded
  std::string debug(){
    return std::format("path:{},{}",path,format.debug());
  }
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