include_directories(../src)
include_directories(checkers)
include_directories(../deps/dice/test/include)

include_directories(../deps/dice/include)

add_library(trace_checker SHARED checkers/trace_checker.c)
target_link_libraries(trace_checker PRIVATE vsync coldtrace-unboxed)
add_library(trace_checker_mock SHARED checkers/trace_checker_mock.c)
target_link_libraries(trace_checker_mock PRIVATE vsync)

set(OPTIONS_atomic_test -ensure-traces)
set(OPTIONS_spinlock -ensure-traces)

file(GLOB SRCS *.c *.cpp)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)

  set(TRACES_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}__traces")

  add_executable(${TARGET} ${SRC})
  target_compile_options(${TARGET} PRIVATE -fsanitize=thread)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET} PRIVATE -Wno-use-after-free)
  endif()
  target_link_libraries(${TARGET} PRIVATE tsano vsync pthread)

  if(${TARGET} MATCHES "^trace_.*")
    target_link_libraries(${TARGET} PRIVATE trace_checker_mock)
    add_test(NAME ${TARGET}-test
             COMMAND ${PROJECT_SOURCE_DIR}/scripts/coldtrace-check ./${TARGET})
  else()
    add_test(
      NAME ${TARGET}-test
      COMMAND
        env COLDTRACE_PATH=${TRACES_DIR}
        ${PROJECT_SOURCE_DIR}/scripts/coldtracer ${OPTIONS_${TARGET}}
        ./${TARGET})
  endif()
endforeach()
target_compile_options(trace_fence PRIVATE -fsanitize=thread -Wno-error=tsan
                                           -Wno-unknown-warning-option)
