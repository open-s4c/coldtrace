cmake_minimum_required(VERSION 3.20)
project(
  coldtrace
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(deps/dice)
find_package(libvsync 4.2 CONFIG)
if(NOT libvsync_DIR)
  set(CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_BINARY_DIR}/deps)
  find_package(libvsync 4.2 CONFIG REQUIRED)
endif()
add_library(vsync ALIAS libvsync::vsync)

add_compile_options(-fPIC -g3 -D_GNU_SOURCE)
add_compile_options(
  -Wall
  -Werror
  -Wextra
  -Wno-uninitialized
  -Wno-unused-variable
  -Wno-unused-parameter
  -Wno-unused-function
  -Wno-ignored-qualifiers
  -Wno-frame-address
  -Wno-unused-command-line-argument)
include_directories(include)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)
