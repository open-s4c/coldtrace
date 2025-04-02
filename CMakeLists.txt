cmake_minimum_required(VERSION 3.24)
project(
  coldtrace
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libvsync 4.2 CONFIG REQUIRED)
add_library(vsync INTERFACE IMPORTED)

find_package(bingo 0.1 CONFIG REQUIRED)
add_library(bingo INTERFACE IMPORTED)

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
  -Wno-frame-address)
include_directories(include)
add_subdirectory(src)
add_subdirectory(test)
