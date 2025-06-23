# Initialization order:
#
# * 200: coldtrace.o
# * 201: self.o
# * 202: procmaps.o
# * 203: rest of coldtrace
#
# coldtrace must initilize before self.o because coldtrace_config() must set the
# path pattern before events in CAPTURE_* chains start arriving from self.o.
set(PRIO_coldtrace 200)
set(PRIO_procmaps 202)

set(SRCS coldtrace.c procmaps.c writer.c single.cpp)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_library(${TARGET}.o OBJECT ${SRC})
  target_link_libraries(${TARGET}.o PUBLIC vsync dice.h)
endforeach()

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

set(MODS ${MODS} dice.o coldtrace.o procmaps.o)
foreach(TARGET ${MODS})
  get_filename_component(MOD ${TARGET} NAME_WLE)
  target_link_libraries(${TARGET} PUBLIC vsync dice.h)
  set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  if(PRIO_${MOD})
    target_compile_options(${TARGET} PRIVATE -DDICE_XTOR_PRIO=${PRIO_${MOD}})
  endif()
endforeach()

# There are 2 libraries: coldtrace uses a handwritten dispatch function and is
# the default library. coldtrace2 is experimental and uses the automatically
# generated dice-fatsch dispatcher.
add_library(coldtrace SHARED single.cpp writer.c)
set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace PUBLIC vsync dice.h)
target_compile_options(coldtrace PRIVATE -DDICE_XTOR_PRIO=203)
target_link_libraries(coldtrace PUBLIC ${MODS})

add_library(
  coldtrace2 SHARED writer.c intercept_pthread.cpp intercept_malloc.cpp
                    intercept_sem.cpp intercept_cxa.cpp intercept_tsan.cpp)
set_property(TARGET coldtrace2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace2 PUBLIC vsync dice.h)
target_compile_options(coldtrace2 PRIVATE -DDICE_XTOR_PRIO=203)
target_link_libraries(coldtrace2 PUBLIC ${MODS} dice-fastch.o)
