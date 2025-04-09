cmake_minimum_required(VERSION 3.24)
project(
  coldtrace
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)

find_package(libvsync 4.2 CONFIG)
if(NOT libvsync_DIR)
  ExternalProject_Add(
    libvsync-deps
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/target
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/libvsync
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/libvsync
    PATCH_COMMAND git checkout . && git apply
                  ${CMAKE_CURRENT_SOURCE_DIR}/deps/libvsync.patch
    INSTALL_COMMAND "")
endif()

find_package(bingo 0.1 CONFIG)
if(NOT bingo_DIR)
  ExternalProject_Add(
    bingo-deps
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/target
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/bingo
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/bingo
    PATCH_COMMAND git checkout . && git apply
                  ${CMAKE_CURRENT_SOURCE_DIR}/deps/bingo.patch
    INSTALL_COMMAND "")
endif()

add_library(vsync INTERFACE IMPORTED)
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
