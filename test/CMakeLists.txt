include_directories(../src)
include_directories(checkers)
include_directories(../deps/dice/test/include)

add_library(trace_checker SHARED checkers/trace_checker.c)
target_link_libraries(trace_checker PUBLIC vsync dice.h ${COLDTRACE_OBJS})

add_library(mock_checker SHARED checkers/mock_checker.c)
target_link_libraries(mock_checker PUBLIC vsync dice.h)

set(OPTIONS_atomic_test -ensure-traces)
set(OPTIONS_spinlock -ensure-traces)

file(GLOB SRCS *.c *.cpp)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  set(TRACES_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}__traces")

  add_executable(${TARGET} ${SRC})
  target_link_libraries(${TARGET} PRIVATE dice.h vsync pthread)

  # handle instrumentation and linking of target against TSAN
  target_compile_options(${TARGET} PRIVATE -fsanitize=thread)
  target_link_options(${TARGET} PRIVATE -fsanitize=thread)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET} PRIVATE -Wno-use-after-free)
  elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${TARGET} PRIVATE -shared-libsan)
  endif()

  # if this is a trace test, we also link with mock_checker and set the test
  # runner accordingly
  if(${TARGET} MATCHES "^trace_.*")
    target_link_libraries(${TARGET} PRIVATE mock_checker)
    add_test(NAME ${TARGET}-test
             COMMAND ${PROJECT_SOURCE_DIR}/scripts/coldtrace-check
                     ${CMAKE_CURRENT_BINARY_DIR}/${TARGET})
  else()
    add_test(
      NAME ${TARGET}-test
      COMMAND
        env COLDTRACE_PATH=${TRACES_DIR}
        ${PROJECT_SOURCE_DIR}/scripts/coldtrace ${OPTIONS_${TARGET}}
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET})
  endif()
endforeach()

target_link_libraries(trace_hb_check PRIVATE entries.o)
target_compile_options(trace_fence PRIVATE -fsanitize=thread -Wno-error=tsan
                                           -Wno-unknown-warning-option)
# force a call to memcpy or __tsan_memcpy
target_compile_options(trace_memcpy PRIVATE -fno-builtin)
set_tests_properties(trace_fail-test PROPERTIES WILL_FAIL TRUE)
