cmake_minimum_required(VERSION 3.22)

project(resource_server)

set(CMAKE_CXX_STANDARD 23)

# make sure i use boost 1.89 built from source.
set(BOOST_ROOT "/usr/local")
set(Boost_INCLUDE_DIR "/usr/local/include")
set(Boost_LIBRARY_DIR "/usr/local/lib")

find_package(Boost REQUIRED COMPONENTS
  system
  filesystem
  thread
  asio
  program_options
  json
)

include_directories(include)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(resource_server ${SOURCES})

link_libraries(${MySQL_LIBRARIES})
link_libraries(${Boost_LIBRARIES})