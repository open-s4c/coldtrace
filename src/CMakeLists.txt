set(SRCS
    single.cpp writer.c
    # coldtrace.cpp intercept_pthread.cpp intercept_malloc.cpp intercept_sem.cpp
    # intercept_cxa.cpp intercept_tsan.cpp
)

add_library(coldtrace SHARED ${SRCS})
target_link_libraries(coldtrace PUBLIC vsync dice.h)
target_compile_options(coldtrace PRIVATE -DDICE_XTOR_PRIO=202 -DDICE_LTO)
set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set(MODS
    mod-self.o
    # pthread library
    mod-pthread_create.o
    mod-pthread_mutex.o
    mod-pthread_rwlock.o
    mod-pthread_cond.o
    # malloc free and friends
    mod-malloc.o
    # semaphore
    mod-sem.o
    # cxa guards
    mod-cxa.o
    # tsan instrumented calls
    mod-tsan.o
    mod-stacktrace.o)

set(OBJS ${MODS} dice.o)
foreach(TARGET ${OBJS})
  get_filename_component(MOD ${TARGET} NAME_WLE)
  if(PRIO_${MOD})
    target_compile_options(${TARGET} PRIVATE -DDICE_XTOR_PRIO=${PRIO_${MOD}})
  endif()
  set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  target_compile_options(${TARGET} PRIVATE -DDICE_LTO)
endforeach()
target_link_libraries(coldtrace PUBLIC ${OBJS})
