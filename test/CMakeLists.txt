# Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# SPDX-License-Identifier: 0BSD

include_directories(../src)
include_directories(checkers)

add_library(trace_checker SHARED checkers/trace_checker.c)
target_link_libraries(trace_checker PUBLIC vsync coldtrace-unboxed)

file(GLOB SRCS *.c *.cpp)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_executable(${TARGET} ${SRC})
  target_compile_options(${TARGET} PRIVATE -fsanitize=thread)
  target_link_libraries(${TARGET} PRIVATE tsano vsync pthread)
  if(${TARGET} MATCHES "^trace_.*")
    target_link_libraries(${TARGET} PRIVATE trace_checker)
    add_test(NAME ${TARGET}-test
             COMMAND ${PROJECT_SOURCE_DIR}/scripts/coldtrace-check ./${TARGET})
  else()
    add_test(NAME ${TARGET}-test
             COMMAND env COLDTRACE_PATH=/tmp
                     ${PROJECT_SOURCE_DIR}/scripts/coldtracer ./${TARGET})
  endif()
endforeach()
