# Initialization order:
#
# * 200: coldtrace.o
# * 201: self.o
# * 202: procmaps.o
# * 203: rest of coldtrace
#
# coldtrace must initilize before self.o because coldtrace_config() must set the
# path pattern before events in CAPTURE_* chains start arriving from self.o.
set(PRIO_coldtrace 3)
set(PRIO_procmaps 5)
set(PRIO_intercept_sem 6)
set(PRIO_intercept_tsan 7)
set(PRIO_intercept_pthread 8)
set(PRIO_intercept_malloc 9)
set(PRIO_intercept_cxa 10)
set(PRIO_intercept_mman 11)

# set(PRIO_pthread_mutex 204) set(PRIO_pthread_rwlock 205) set(PRIO_pthread_cond
# 206) set(PRIO_malloc 207) set(PRIO_sem 208) set(PRIO_cxa 209) set(PRIO_tsan
# 210) set(PRIO_stacktrace 211) set(PRIO_pthread_create 212)

set(SRCS
    coldtrace.c
    procmaps.c
    writer.c
    coldthread.cpp
    intercept_sem.cpp
    intercept_tsan.cpp
    intercept_pthread.cpp
    intercept_malloc.cpp
    intercept_mman.cpp
    intercept_cxa.cpp)

set(MODS)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_library(${TARGET}.o OBJECT ${SRC})
  target_link_libraries(${TARGET}.o PUBLIC vsync dice.h)
  if(PRIO_${TARGET})
    target_compile_options(${TARGET}.o
                           PRIVATE -DDICE_MODULE_PRIO=${PRIO_${TARGET}})
    target_compile_options(${TARGET}.o PRIVATE -DDICE_BUILTIN)
  endif()
  set(MODS ${MODS} ${TARGET}.o)
endforeach()

set(MODS
    ${MODS}
    dice-self.o
    dice-pthread_create.o
    dice-pthread_mutex.o
    dice-pthread_rwlock.o
    dice-pthread_cond.o
    dice-malloc.o
    dice-mman.o
    dice-sem.o
    dice-cxa.o
    dice-tsan.o
    dice-stacktrace.o)

set(MODS ${MODS} dice.o dice-box.o)

# There are 2 libraries: coldtrace uses a handwritten dispatch function and is
# the default library. coldtrace2 is experimental and uses the automatically
# generated dice-fatsch dispatcher.
add_library(coldtrace SHARED)
set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace PUBLIC vsync dice.h)
# target_compile_options(coldtrace PRIVATE -DDICE_XTOR_PRIO=203)
target_link_libraries(coldtrace PUBLIC ${MODS} writer.o)

# add_library( coldtrace2 SHARED writer.c intercept_pthread.cpp
# intercept_malloc.cpp intercept_sem.cpp intercept_cxa.cpp intercept_tsan.cpp)
# set_property(TARGET coldtrace2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
# target_link_libraries(coldtrace2 PUBLIC vsync dice.h)
# target_compile_options(coldtrace2 PRIVATE -DDICE_XTOR_PRIO=203)
# target_link_libraries(coldtrace2 PUBLIC ${MODS} dice-fastch.o)
