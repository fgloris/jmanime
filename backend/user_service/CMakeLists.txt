cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 23)
project(user_service)

set(CMAKE_BUILD_TYPE Debug)
set(Protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(UUID REQUIRED uuid)
pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)

file(GLOB_RECURSE SOURCES 
  "application/*.cpp" "domain/*.cpp" "infrastructure/*.cpp" "interface/*.cpp"
  "../common/*.cpp"
)
add_executable(${PROJECT_NAME} 
  ${SOURCES}
  ${PROTO_SRCS}
  main.cpp
)

add_dependencies(${PROJECT_NAME} proto_gen)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${Protobuf_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROTO_GEN_DIR}
  ${UUID_INCLUDE_DIRS}
  ${MYSQLCLIENT_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${UUID_LIBRARIES}
  ${Boost_LIBRARIES}
  ${MYSQLCLIENT_LIBRARIES}
  ${HIREDIS_LIBRARIES}
  protobuf::libprotobuf
  gRPC::grpc
  gRPC::grpc++
  jwt-cpp
  uuid
)