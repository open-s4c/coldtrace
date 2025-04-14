set(SRCS
    coldtrace.cpp
    writer.c
    intercept_pthread.cpp
    intercept_malloc.cpp
    intercept_sem.cpp
    intercept_cxa.cpp
    intercept_tsan.cpp)

add_library(coldtrace SHARED ${SRCS})
target_link_libraries(coldtrace PUBLIC vsync bingo)

add_library(coldtrace.lto SHARED ${SRCS})
target_link_libraries(coldtrace.lto PUBLIC vsync bingo.h)
target_link_libraries(coldtrace.lto PRIVATE -Wl,--whole-archive bingo.a
                                            -Wl,--no-whole-archive)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  set_property(TARGET coldtrace.lto PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
  target_compile_options(coldtrace PRIVATE -fPIC -flto -ffat-lto-objects)
  target_compile_options(coldtrace.lto PRIVATE -fPIC -flto -ffat-lto-objects)
endif()
