cmake_minimum_required(VERSION 3.20)
project(bingo-installer)
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
      ${CMAKE_CURRENT_SOURCE_DIR}/bingo.patch)
endif()

ExternalProject_Add(
  bingo-deps
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bingo
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/build
  PATCH_COMMAND ${PCMD}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -DCMAKE_FIND_ROOT_PATH=${CMAKE_INSTALL_PREFIX})
