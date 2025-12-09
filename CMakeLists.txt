cmake_minimum_required(VERSION 3.20)
project(
  coldtrace
  VERSION 0.1
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(LOG_PREFIX="coldtrace: ")
# Platform specific interfaces
set(HAVE_PTHREAD_CLOCKLOCK_CLOCKWAIT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(READ "/etc/os-release" _os)
  string(FIND "${_os}" "ID=alpine" _alpine)
  if(_alpine EQUAL -1)
    set(HAVE_PTHREAD_CLOCKLOCK_CLOCKWAIT ON)
  endif()
elseif(NOT CMAKE_SYSTEM_NAME MATCHES "NetBSD" AND NOT APPLE)
  set(HAVE_PTHREAD_CLOCKLOCK_CLOCKWAIT ON)
endif()

add_compile_options(-fPIC -g3 -D_GNU_SOURCE -DDICE_MAIN_START_DISABLE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_subdirectory(deps/dice)
find_package(libvsync 4.2 CONFIG)
if(NOT libvsync_DIR)
  set(CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_BINARY_DIR}/deps)
  find_package(libvsync 4.2 CONFIG REQUIRED)
endif()
add_library(vsync ALIAS libvsync::vsync)

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

file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/test/*.h
    ${CMAKE_SOURCE_DIR}/test/*.cpp
    ${CMAKE_SOURCE_DIR}/test/*.c)

add_custom_target(format
    COMMAND clang-format -style=file -i ${ALL_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on all source files")

add_custom_target(diff-check COMMAND git --no-pager diff --exit-code)
