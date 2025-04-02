cmake_minimum_required(VERSION 3.20)
project(libvsync-installer)
include(ExternalProject)
message(STATUS "Running ${PROJECT_NAME}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libvsync.patch")
  set(PCMD
      git
      checkout
      .
      &&
      git
      apply
      ${CMAKE_CURRENT_SOURCE_DIR}/libvsync.patch)
endif()

ExternalProject_Add(
  libvsync-deps
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libvsync
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/build
  PATCH_COMMAND ${PCMD}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})
