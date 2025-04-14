set(SRCS
    single.cpp writer.c
    # coldtrace.cpp intercept_pthread.cpp intercept_malloc.cpp intercept_sem.cpp
    # intercept_cxa.cpp intercept_tsan.cpp
)

add_library(coldtrace SHARED ${SRCS})
target_link_libraries(coldtrace PUBLIC vsync bingo.lto)

if(${CMAKE_SYSTEM_NAME} STREQUAL "NetBSD")
  target_compile_options(coldtrace PRIVATE -fPIC -flto -ffat-lto-objects)
else()
  # this should work for Linux, macOS and OHOS
  set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
