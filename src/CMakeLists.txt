if (NOT DEFINED COLDTRACE_BASE_PRIO)
    set(COLDTRACE_BASE_PRIO 3 CACHE PATH "base prio" FORCE)
endif()

MATH(EXPR PRIO_coldtrace "${COLDTRACE_BASE_PRIO} + 1")
MATH(EXPR PRIO_mod-self "${COLDTRACE_BASE_PRIO} + 0")
MATH(EXPR PRIO_procmaps "${COLDTRACE_BASE_PRIO} + 2")
MATH(EXPR PRIO_intercept_sem "${COLDTRACE_BASE_PRIO} + 3")
MATH(EXPR PRIO_intercept_tsan "${COLDTRACE_BASE_PRIO} + 4")
MATH(EXPR PRIO_intercept_pthread "${COLDTRACE_BASE_PRIO} + 5")
MATH(EXPR PRIO_intercept_malloc "${COLDTRACE_BASE_PRIO} + 6")
MATH(EXPR PRIO_intercept_cxa "${COLDTRACE_BASE_PRIO} + 7")
MATH(EXPR PRIO_intercept_mman "${COLDTRACE_BASE_PRIO} + 8")

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
                           PRIVATE -DCOLDTRACE_BASE_PRIO=${COLDTRACE_BASE_PRIO} -DDICE_MODULE_PRIO=${PRIO_${TARGET}})
    target_compile_options(${TARGET}.o PRIVATE -DDICE_BUILTIN)
  endif()
  set(MODS ${MODS} ${TARGET}.o)
endforeach()

target_compile_options(mod-self.o PRIVATE -DCOLDTRACE_BASE_PRIO=${COLDTRACE_BASE_PRIO} -DDICE_MODULE_PRIO=${PRIO_mod-self})

set(MODS
    ${MODS}
    mod-self.o
    mod-pthread_create.o
    mod-pthread_mutex.o
    mod-pthread_rwlock.o
    mod-pthread_cond.o
    mod-malloc.o
    mod-mman.o
    mod-sem.o
    mod-cxa.o
    mod-tsan.o
    mod-stacktrace.o)

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
