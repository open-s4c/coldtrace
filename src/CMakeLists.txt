set(MODS
    mod-self.o
    mod-pthread_create.o
    mod-pthread_mutex.o
    mod-pthread_rwlock.o
    mod-pthread_cond.o
    mod-malloc.o
    mod-sem.o
    mod-cxa.o
    mod-tsan.o
    mod-stacktrace.o)

set(MODS ${MODS} dice.o)
foreach(TARGET ${MODS})
  get_filename_component(MOD ${TARGET} NAME_WLE)
  if(PRIO_${MOD})
    target_compile_options(${TARGET} PRIVATE -DDICE_XTOR_PRIO=${PRIO_${MOD}})
  endif()
  set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endforeach()

set(SRCS
    writer.c
    coldthread.cpp
    intercept_pthread.cpp
    intercept_malloc.cpp
    intercept_sem.cpp
    intercept_cxa.cpp
    intercept_tsan.cpp)

add_library(coldtrace.o OBJECT coldtrace.cpp)
set_property(TARGET coldtrace.o PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace.o PUBLIC vsync dice.h)
target_compile_options(coldtrace.o PRIVATE -DDICE_XTOR_PRIO=201)

add_library(coldtrace2 SHARED ${SRCS})
set_property(TARGET coldtrace2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace2 PUBLIC vsync dice.h)
target_compile_options(coldtrace2 PRIVATE -DDICE_XTOR_PRIO=202)
target_link_libraries(coldtrace2 PUBLIC coldtrace.o ${MODS} dice-fastch.o)

add_library(coldtrace SHARED single.cpp writer.c)
set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace PUBLIC vsync dice.h)
target_compile_options(coldtrace PRIVATE -DDICE_XTOR_PRIO=202)
target_link_libraries(coldtrace PUBLIC coldtrace.o ${MODS})
