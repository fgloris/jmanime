cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 23)
project(jmanime)

option(DEBUG "Enable debug mode" OFF)

get_filename_component(vid_proto "proto/video.proto" ABSOLUTE)
get_filename_component(user_proto "proto/user.proto" ABSOLUTE)
get_filename_component(vid_proto_path "${vid_proto}" PATH)
get_filename_component(user_proto_path "${vid_proto}" PATH)

set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(vid_proto_srcs "${PROTO_GEN_DIR}/video.pb.cc")
set(vid_proto_hdrs "${PROTO_GEN_DIR}/video.pb.h")
set(vid_grpc_srcs "${PROTO_GEN_DIR}/video.grpc.pb.cc")
set(vid_grpc_hdrs "${PROTO_GEN_DIR}/video.grpc.pb.h")

set(user_proto_srcs "${PROTO_GEN_DIR}/user.pb.cc")
set(user_proto_hdrs "${PROTO_GEN_DIR}/user.pb.h")
set(user_grpc_srcs "${PROTO_GEN_DIR}/user.grpc.pb.cc")
set(user_grpc_hdrs "${PROTO_GEN_DIR}/user.grpc.pb.h")

find_program(PROTOBUF_PROTOC protoc)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
message(STATUS "find protoc: ${PROTOBUF_PROTOC}, gRPC plugin: ${GRPC_CPP_PLUGIN_EXECUTABLE}")

add_custom_command(
  OUTPUT "${vid_proto_srcs}" "${vid_proto_hdrs}" "${vid_grpc_srcs}" "${vid_grpc_hdrs}"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS --grpc_out "${PROTO_GEN_DIR}"
  --cpp_out "${PROTO_GEN_DIR}"
  -I "${vid_proto_path}"
  --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
  "${vid_proto}"
  DEPENDS "${vid_proto}"
  COMMENT "Generating video proto files..."
)

add_custom_command(
  OUTPUT "${user_proto_srcs}" "${user_proto_hdrs}" "${user_grpc_srcs}" "${user_grpc_hdrs}"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS --grpc_out "${PROTO_GEN_DIR}"
  --cpp_out "${PROTO_GEN_DIR}"
  -I "${user_proto_path}"
  --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
  "${user_proto}"
  DEPENDS "${user_proto}"
  COMMENT "Generating user proto files..."
)

add_custom_target(proto_gen DEPENDS
  ${vid_proto_srcs}
  ${vid_proto_hdrs}
  ${vid_grpc_srcs}
  ${vid_grpc_hdrs}
  ${user_proto_srcs}
  ${user_proto_hdrs}
  ${user_grpc_srcs}
  ${user_grpc_hdrs}
)

set(PROTO_SRCS 
  ${vid_proto_srcs} 
  ${vid_grpc_srcs}
  ${user_proto_srcs}
  ${user_grpc_srcs}
)
set(PROTO_HDRS 
  ${vid_proto_hdrs} 
  ${vid_grpc_hdrs}
  ${user_proto_hdrs}
  ${user_grpc_hdrs}
)

add_subdirectory(third_parties/jwt-cpp)
add_subdirectory(video_service)
add_subdirectory(user_service)
