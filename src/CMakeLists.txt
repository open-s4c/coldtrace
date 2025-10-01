if(NOT DEFINED COLDTRACE_BASE_SLOT)
  set(COLDTRACE_BASE_SLOT
      3
      CACHE PATH "base prio" FORCE)
endif()

math(EXPR SLOT_config "${COLDTRACE_BASE_SLOT} + 1")
math(EXPR SLOT_mod-self "${COLDTRACE_BASE_SLOT} + 0")
math(EXPR SLOT_procmaps "${COLDTRACE_BASE_SLOT} + 2")
math(EXPR SLOT_subs_sem "${COLDTRACE_BASE_SLOT} + 3")
math(EXPR SLOT_subs_tsan "${COLDTRACE_BASE_SLOT} + 4")
math(EXPR SLOT_subs_pthread "${COLDTRACE_BASE_SLOT} + 5")
math(EXPR SLOT_subs_malloc "${COLDTRACE_BASE_SLOT} + 6")
math(EXPR SLOT_subs_cxa "${COLDTRACE_BASE_SLOT} + 7")
math(EXPR SLOT_subs_mman "${COLDTRACE_BASE_SLOT} + 8")
math(EXPR SLOT_subs_annotate_rwlock "${COLDTRACE_BASE_SLOT} + 9")
math(EXPR SLOT_subs_memcpy "${COLDTRACE_BASE_SLOT} + 10")

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
    subs_memcpy.c
    subs_cxa.c
    subs_annotate_rwlock.c)

set(MODS)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_library(${TARGET}.o OBJECT ${SRC})
  target_link_libraries(${TARGET}.o PUBLIC vsync dice.h)
  if(SLOT_${TARGET})
    target_compile_options(
      ${TARGET}.o PRIVATE -DCOLDTRACE_BASE_SLOT=${COLDTRACE_BASE_SLOT}
                          -DDICE_MODULE_SLOT=${SLOT_${TARGET}})
    target_compile_options(${TARGET}.o PRIVATE -DDICE_BUILTIN)
  endif()
  set(MODS ${MODS} ${TARGET}.o)
endforeach()

target_compile_options(
  dice-self.o PRIVATE -DCOLDTRACE_BASE_SLOT=${COLDTRACE_BASE_SLOT}
                      -DDICE_MODULE_SLOT=${SLOT_mod-self})

set(MODS
    ${MODS}
    dice-pthread_create.o
    dice-pthread_mutex.o
    dice-pthread_rwlock.o
    dice-pthread_cond.o
    dice-malloc.o
    dice-mman.o
    dice-memcpy.o
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
