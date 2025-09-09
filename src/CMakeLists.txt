if(NOT DEFINED COLDTRACE_BASE_PRIO)
  set(COLDTRACE_BASE_PRIO
      3
      CACHE PATH "base prio" FORCE)
endif()

math(EXPR PRIO_coldtrace "${COLDTRACE_BASE_PRIO} + 1")
math(EXPR PRIO_mod-self "${COLDTRACE_BASE_PRIO} + 0")
math(EXPR PRIO_procmaps "${COLDTRACE_BASE_PRIO} + 2")
math(EXPR PRIO_intercept_sem "${COLDTRACE_BASE_PRIO} + 3")
math(EXPR PRIO_intercept_tsan "${COLDTRACE_BASE_PRIO} + 4")
math(EXPR PRIO_intercept_pthread "${COLDTRACE_BASE_PRIO} + 5")
math(EXPR PRIO_intercept_malloc "${COLDTRACE_BASE_PRIO} + 6")
math(EXPR PRIO_intercept_cxa "${COLDTRACE_BASE_PRIO} + 7")
math(EXPR PRIO_intercept_mman "${COLDTRACE_BASE_PRIO} + 8")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION true)

set(SRCS
    coldtrace.c
    procmaps.c
    writer.c
    events.c
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
    target_compile_options(
      ${TARGET}.o PRIVATE -DCOLDTRACE_BASE_PRIO=${COLDTRACE_BASE_PRIO}
                          -DDICE_MODULE_PRIO=${PRIO_${TARGET}})
    target_compile_options(${TARGET}.o PRIVATE -DDICE_BUILTIN)
  endif()
  set(MODS ${MODS} ${TARGET}.o)
endforeach()

target_compile_options(
  dice-self.o PRIVATE -DCOLDTRACE_BASE_PRIO=${COLDTRACE_BASE_PRIO}
                      -DDICE_MODULE_PRIO=${PRIO_mod-self})

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
    dice-stacktrace.o
    dice.o)

add_library(coldtrace SHARED)
set_property(TARGET coldtrace PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(coldtrace PUBLIC vsync dice.h)
target_link_libraries(coldtrace PUBLIC ${MODS} dice-box.o writer.o)

# useful for testing (allows loading additional shared libraries)
add_library(coldtrace-unboxed SHARED)
target_link_libraries(coldtrace-unboxed PUBLIC vsync dice.h dl)
target_link_libraries(coldtrace-unboxed PUBLIC ${MODS} writer.o)
