cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 23)
project(user_service)

set(Protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)
pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)

# Main executable
file(GLOB_RECURSE SOURCES "application/*.cpp" "domain/*.cpp" "infrastructure/*.cpp" "interface/*.cpp")
add_executable(${PROJECT_NAME} 
  ${SOURCES}
  ${PROTO_SRCS}
  main.cpp
)

add_dependencies(${PROJECT_NAME} proto_gen)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Protobuf_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTO_GEN_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${UUID_INCLUDE_DIRS}
  ${MYSQLCLIENT_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  protobuf::libprotobuf
  gRPC::grpc
  gRPC::grpc++
  OpenSSL::SSL
  OpenSSL::Crypto
  ${UUID_LIBRARIES}
  ${MYSQLCLIENT_LIBRARIES}
  jwt-cpp
)

add_executable(${PROJECT_NAME}_client client.cpp ${PROTO_SRCS})

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
