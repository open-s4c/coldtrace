cmake_minimum_required(VERSION 3.20)
project(
  coldtrace
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libvsync 4.2 CONFIG)
find_package(bingo 0.1 CONFIG)
if(NOT libvsync_DIR OR NOT bingo_DIR)
  execute_process(
    COMMAND
      ${CMAKE_COMMAND} #
      -S${CMAKE_CURRENT_SOURCE_DIR}/deps #
      -B${CMAKE_CURRENT_BINARY_DIR}/deps-build
      -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/deps
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/deps-build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  set(CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_BINARY_DIR}/deps)

  find_package(libvsync 4.2 CONFIG REQUIRED)
  find_package(bingo 0.1 CONFIG REQUIRED)
endif()
add_library(vsync ALIAS libvsync::vsync)
add_library(bingo ALIAS bingo::bingo)

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
