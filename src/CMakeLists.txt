if(NOT DEFINED COLDTRACE_BASE_PRIO)
  set(COLDTRACE_BASE_PRIO
      3
      CACHE PATH "base prio" FORCE)
endif()

math(EXPR PRIO_config "${COLDTRACE_BASE_PRIO} + 1")
math(EXPR PRIO_mod-self "${COLDTRACE_BASE_PRIO} + 0")
math(EXPR PRIO_procmaps "${COLDTRACE_BASE_PRIO} + 2")
math(EXPR PRIO_subs_sem "${COLDTRACE_BASE_PRIO} + 3")
math(EXPR PRIO_subs_tsan "${COLDTRACE_BASE_PRIO} + 4")
math(EXPR PRIO_subs_pthread "${COLDTRACE_BASE_PRIO} + 5")
math(EXPR PRIO_subs_malloc "${COLDTRACE_BASE_PRIO} + 6")
math(EXPR PRIO_subs_cxa "${COLDTRACE_BASE_PRIO} + 7")
math(EXPR PRIO_subs_mman "${COLDTRACE_BASE_PRIO} + 8")
math(EXPR PRIO_subs_annotate_rwlock "${COLDTRACE_BASE_PRIO} + 9")

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION true)

# main functionality
set(SRCS
    thread.cpp
    procmaps.c
    counters.c
    config.c
    utils.c
    writer.c
    entries.c)

# subscribers
set(SRCS
    ${SRCS}
    subs_sem.c
    subs_tsan.c
    subs_pthread.c
    subs_malloc.c
    subs_mman.c
    subs_cxa.c
    subs_annotate_rwlock.c)

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
    dice-annotate_rwlock.o
    dice.o
    dice-box.o
    dice-self.o
    dice-self-box.o
    writer.o)

add_library(coldtrace SHARED)
target_link_libraries(coldtrace PUBLIC vsync dice.h)
target_link_libraries(coldtrace PUBLIC ${MODS})

set(COLDTRACE_OBJS
    ${MODS}
    PARENT_SCOPE)
