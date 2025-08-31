```mermaid
erDiagram
  User {
    string user_id PK
    string user_name
    string user_avatar
    string user_email
    string user_passwd_hash
    string user_passwd_salt
  }

  VideoStorage {
    string video_id PK
    string video_path
    string video_format
    string video_codec
    int video_width
    int video_height
    int video_bitrate
  }
  
  VideoPresentInfo {
    string video_id PK
    string video_title
    string video_description
    string video_duration
  }
    
  WatchHistory {
    string video_id PK
    string user_id PK
    time watch_time
  }

  UploadHistory {
    string video_id PK
    string user_id PK
    time upload_time
  }
  
  VideoPresentInfo ||--|| VideoStorage : has
  User }o--|| WatchHistory : has
  WatchHistory ||--o{ VideoPresentInfo : has
  User }o--|| UploadHistory : has
  UploadHistory ||--o{ VideoPresentInfo : has
```