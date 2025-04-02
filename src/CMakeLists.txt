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
