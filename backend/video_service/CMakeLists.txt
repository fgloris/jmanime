cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 23)
project(video_service)

set(Protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS
  filesystem
)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)

message(STATUS "Using boost ${Boost_VERSION}, gRPC ${gRPC_VERSION}, Protobuf ${Protobuf_VERSION}")

file(GLOB_RECURSE SOURCES 
  "application/*.cpp" "domain/*.cpp" "infrastructure/*.cpp" "interface/*.cpp" "../common/config.cpp"
)
add_executable(${PROJECT_NAME} 
  ${SOURCES}
  ${PROTO_SRCS}
  main.cpp
)

add_dependencies(${PROJECT_NAME} proto_gen)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${Protobuf_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTO_GEN_DIR}
  ${UUID_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${Boost_LIBRARIES}
  ${FFMPEG_LIBRARIES}
  ${MYSQLCLIENT_LIBRARIES}
  ${CURL_LIBRARIES}
  protobuf::libprotobuf
  gRPC::grpc
  gRPC::grpc++
  OpenSSL::SSL
  OpenSSL::Crypto
  uuid
)

add_executable(${PROJECT_NAME}_client client.cpp ${PROTO_SRCS} "../common/config/config.cpp")

target_include_directories(${PROJECT_NAME}_client PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Protobuf_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTO_GEN_DIR}
)

target_link_libraries(${PROJECT_NAME}_client PRIVATE
  protobuf::libprotobuf
  gRPC::grpc
  gRPC::grpc++
)

# Add transcoder test executable
add_executable(test_transcoder testtranscoder.cpp infrastructure/mp4_transcoder.cpp "../common/config/config.cpp")

target_include_directories(test_transcoder PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(test_transcoder PRIVATE
  ${FFMPEG_LIBRARIES}
)
