add_library(
  coldtrace SHARED
  coldtrace.cpp
  writer.c
  intercept_pthread.cpp
  intercept_malloc.cpp
  intercept_sem.cpp
  intercept_cxa.cpp
  intercept_tsan.cpp)
target_link_libraries(coldtrace PUBLIC vsync bingo)

add_library(
  coldtrace.o OBJECT coldtrace.cpp intercept_pthread.cpp intercept_malloc.cpp
                     intercept_sem.cpp intercept_cxa.cpp intercept_tsan.cpp)
target_link_libraries(coldtrace.o PUBLIC vsync bingo.h)

add_library(coldtrace.lto SHARED writer.c)
target_link_libraries(
  coldtrace.lto PRIVATE -Wl,--whole-archive bingo.a -Wl,--no-whole-archive #
                        coldtrace.o)
