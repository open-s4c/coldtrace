include_directories(../src)
include_directories(checkers)
include_directories("${DICE_SOURCE}/test/include")

find_package(Python3 3.10 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME trace_dump-test COMMAND ${Python3_EXECUTABLE} -m unittest
                                        test.test_trace_dump)
  set_tests_properties(trace_dump-test PROPERTIES WORKING_DIRECTORY
                                                  ${PROJECT_SOURCE_DIR})
endif()

add_library(trace_checker SHARED checkers/trace_checker.c
                                 checkers/indexes_checker.c)
target_link_libraries(trace_checker PRIVATE vsync coldtrace-dice
                                            ${COLDTRACE_OBJS})

# Resolve sanitizer annotations and test hooks supplied by the test program.
if(APPLE)
  target_link_options(trace_checker PRIVATE -undefined dynamic_lookup)
endif()

# Load the C++ runtime for the cxa_guard interceptors.
if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "OHOS")
  set_target_properties(trace_checker PROPERTIES LINKER_LANGUAGE CXX)
endif()

add_dependencies(trace_checker coldtrace-preload)

add_library(mock_checker SHARED checkers/mock_checker.c)
target_link_libraries(mock_checker PUBLIC vsync dice.h)

add_library(marker SHARED checkers/marker.c)

set(OPTIONS_atomic_test -ensure-traces)
set(OPTIONS_spinlock -ensure-traces)

file(GLOB SRCS *.c *.cpp)
if(APPLE)
  # macOS has no POSIX spinlocks, timed mutex/rwlock calls, or sem_timedwait.
  list(
    REMOVE_ITEM
    SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_spinlock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_spintrylock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_mutex_variants.c
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_rwlock_variants.c
    ${CMAKE_CURRENT_SOURCE_DIR}/trace_semaphore.c)
endif()
if(NOT HAVE_PTHREAD_CLOCKLOCK_CLOCKWAIT)
  list(REMOVE_ITEM SRCS
       ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_cond_clockwait.c
       ${CMAKE_CURRENT_SOURCE_DIR}/trace_pthread_mutex_clocklock.c)
endif()
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  set(TRACES_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}__traces")

  add_executable(${TARGET} ${SRC})
  target_link_libraries(${TARGET} PRIVATE dice.h vsync pthread marker)
  if(APPLE)
    target_link_options(${TARGET} PRIVATE LINKER:-flat_namespace
                        LINKER:-undefined,dynamic_lookup)
  endif()

  # handle instrumentation and linking of target against TSAN
  target_compile_options(${TARGET} PRIVATE -O0 -fno-builtin -fsanitize=thread)
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
    set_tests_properties(
      ${TARGET}-test PROPERTIES LABELS "trace" ENVIRONMENT
                                "COLDTRACE_PATH=${TRACES_DIR}")
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
target_link_libraries(trace_hb_check_2 PRIVATE entries.o)
target_link_libraries(trace_stack_growth PRIVATE entries.o)
target_compile_options(trace_stack_growth PRIVATE -fno-optimize-sibling-calls)
target_compile_options(trace_fence PRIVATE -fsanitize=thread -Wno-error=tsan
                                           -Wno-unknown-warning-option)
# force a call to memcpy or __tsan_memcpy
target_compile_options(trace_memcpy PRIVATE -fno-builtin)
set_property(
  TEST trace_fail-test
  APPEND
  PROPERTY ENVIRONMENT "COLDTRACE_EXPECT_FAIL=1")

# Pass locations from the active CMake build, including non-default build dirs.
get_property(
  coldtrace_tests
  DIRECTORY
  PROPERTY TESTS)
foreach(test IN LISTS coldtrace_tests)
  set_property(
    TEST ${test}
    APPEND
    PROPERTY ENVIRONMENT "COLDTRACE_BUILD=${PROJECT_BINARY_DIR}"
             "COLDTRACE_DICE_SOURCE=${DICE_SOURCE}"
             "COLDTRACE_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}")
endforeach()

set_tests_properties(${coldtrace_tests} PROPERTIES TIMEOUT 30)
