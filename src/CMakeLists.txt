set(SRCS
    single.cpp writer.c
    # coldtrace.cpp intercept_pthread.cpp intercept_malloc.cpp intercept_sem.cpp
    # intercept_cxa.cpp intercept_tsan.cpp
)

add_library(coldtrace SHARED ${SRCS})
target_link_libraries(coldtrace PUBLIC vsync bingo.h)
target_compile_options(coldtrace PRIVATE -DBINGO_XTOR_PRIO=201 -DBINGO_LTO
                                         -fPIC -flto -fno-fat-lto-objects)

set(MODS
    self.o
    # pthread library
    pthread_create.o
    pthread_mutex.o
    pthread_rwlock.o
    pthread_cond.o
    # malloc free and friends
    malloc.o
    # semaphore
    sem.o
    # cxa guards
    cxa.o
    # tsan instrumented calls
    tsan.o
    stacktrace.o)

target_link_libraries(coldtrace PUBLIC vsync bingo.h bingo-core.o bingo-base.o)
set(PRIO_self 200)
set(OBJS ${MODS} bingo-core.o bingo-base.o)
foreach(TARGET ${OBJS})
  get_filename_component(MOD ${TARGET} NAME_WLE)
  if(PRIO_${MOD})
    target_compile_options(${TARGET} PRIVATE -DBINGO_XTOR_PRIO=${PRIO_${MOD}})
  endif()
  target_compile_options(${TARGET} PRIVATE -fPIC -flto -fno-fat-lto-objects
                                           -DBINGO_LTO)
endforeach()
target_link_libraries(coldtrace PUBLIC ${OBJS})
