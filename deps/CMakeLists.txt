cmake_minimum_required(VERSION 3.20)
project(deps)
include(ExternalProject)

ExternalProject_Add(
  libvsync-deps
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libvsync
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libvsync
  PATCH_COMMAND git checkout . && git apply
                ${CMAKE_CURRENT_SOURCE_DIR}/libvsync.patch
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})

ExternalProject_Add(
  bingo-deps
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/target
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bingo
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/bingo
  PATCH_COMMAND git checkout . && git apply
                ${CMAKE_CURRENT_SOURCE_DIR}/bingo.patch
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
             -DCMAKE_C_FLAGS=-Wno-unused-result
             -DCMAKE_CXX_FLAGS=-Wno-unused-result
             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
