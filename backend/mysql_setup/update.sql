DROP TABLE IF EXISTS users;

CREATE TABLE users (
  id CHAR(36) PRIMARY KEY,
  email VARCHAR(255) NOT NULL UNIQUE,
  username VARCHAR(255) NOT NULL,
  password_hash CHAR(64) NOT NULL,
  salt CHAR(32) NOT NULL,
  avatar TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) CHARACTER SET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS videos;
CREATE TABLE videos (
  uuid VARCHAR(36) PRIMARY KEY,
  storage_path VARCHAR(255) NOT NULL,
  format_width INT NOT NULL,
  format_height INT NOT NULL,
  format_bitrate INT NOT NULL,
  format_container VARCHAR(10) NOT NULL DEFAULT 'mp4',
  format_codec VARCHAR(10) NOT NULL DEFAULT 'libx265',
  info_duration INT NOT NULL DEFAULT 0,
  info_title VARCHAR(255) NOT NULL,
  info_description TEXT,
  info_url VARCHAR(2048) NOT NULL,
  info_created_at DATETIME NOT NULL,
  info_updated_at DATETIME NOT NULL,
  INDEX idx_created_at (info_created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
