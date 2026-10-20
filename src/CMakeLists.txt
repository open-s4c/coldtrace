# All users, including the trace checker, share one declared configuration.
option(COLDTRACE_CHECK_ROUTES "Check declared Dice routes" OFF)
set(config_modules
    config
    procmaps
    subs_sem
    subs_tsan
    subs_pthread
    subs_malloc
    subs_cxa
    subs_mman
    subs_annotate_rwlock
    subs_memcpy
    subs_marker
    subs_wait
    marker)
set(dice_modules
    self
    pthread_create
    pthread_mutex
    pthread_rwlock
    pthread_cond
    malloc
    mman
    memcpy
    sem
    cxa
    tsan
    stacktrace
    annotate_rwlock)
if(NOT APPLE)
  list(APPEND dice_modules pthread_spinlock)
endif()

# The source configuration declares all Coldtrace slots. Only platform-specific
# imports and ordering constraints are generated.
set(platform_config "")
if(NOT APPLE)
  string(CONCAT platform_config "(import (dice pthread_spinlock))\n"
                "(after (dice pthread_spinlock) (coldtrace marker))\n")
endif()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dice/platform.dice" CONTENT
     "${platform_config}" @ONLY)

add_library(coldtrace-runtime OBJECT)
set(checks)
if(COLDTRACE_CHECK_ROUTES)
  list(APPEND checks CHECK_ROUTES)
endif()
add_dice_core(
  coldtrace-runtime "${PROJECT_SOURCE_DIR}/coldtrace.dice" ${checks}
  IMPORT_PATHS "coldtrace=${CMAKE_CURRENT_BINARY_DIR}/dice")
add_dice_mods(coldtrace-runtime ${dice_modules})
# Coldtrace links all consumers and keeps Dice helpers private, as dice-box did.
target_sources(coldtrace-runtime PRIVATE "${DICE_SOURCE}/src/dice/mempool-box.c"
                                         "${DICE_SOURCE}/src/mod/self-box.c")

# The interface target supplies the generation dependency to each object target.
add_library(coldtrace-dice INTERFACE)
target_link_libraries(coldtrace-dice INTERFACE coldtrace-runtime-dice.h)
get_target_property(rolled_header coldtrace-runtime DICE_ROLL_HEADER)
target_compile_options(coldtrace-dice INTERFACE -include "${rolled_header}")

# main functionality
set(SRCS
    thread.c
    procmaps.c
    counters.c
    config.c
    utils.c
    writer.c
    entries.c
    version.c
    marker.c)

# subscribers
set(SRCS
    ${SRCS}
    subs_sem.c
    subs_tsan.c
    subs_pthread.c
    subs_malloc.c
    subs_marker.c
    subs_mman.c
    subs_memcpy.c
    subs_cxa.c
    subs_annotate_rwlock.c
    subs_wait.c)

set(MODS coldtrace-runtime)
foreach(SRC IN LISTS SRCS)
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_library(${TARGET}.o OBJECT ${SRC})
  target_link_libraries(${TARGET}.o PRIVATE vsync)
  if("${TARGET}" IN_LIST config_modules)
    target_link_libraries(${TARGET}.o PRIVATE coldtrace-dice)
    string(TOUPPER "${TARGET}" slot)
    target_compile_definitions(${TARGET}.o
                               PRIVATE DICE_MODULE_SLOT=SLOT_COLDTRACE_${slot})
  else()
    target_link_libraries(${TARGET}.o PRIVATE dice.h)
  endif()
  list(APPEND MODS ${TARGET}.o)
endforeach()

add_library(coldtrace SHARED)
# The preload runner replaces the sanitizer library before starting a client.
if(APPLE)
  add_custom_target(
    coldtrace-preload
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE:tsano>"
            "$<TARGET_FILE_DIR:tsano>/libclang_rt.tsan_osx_dynamic.dylib"
    DEPENDS tsano
    VERBATIM)
else()
  add_custom_target(coldtrace-preload DEPENDS tsano)
endif()
add_dependencies(coldtrace coldtrace-preload)
target_link_libraries(coldtrace PRIVATE vsync coldtrace-dice ${MODS})
if(APPLE)
  target_link_options(coldtrace PRIVATE -undefined dynamic_lookup)
endif()
if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "OHOS")
  set_target_properties(coldtrace PROPERTIES LINKER_LANGUAGE CXX)
endif()
set(COLDTRACE_OBJS
    ${MODS}
    PARENT_SCOPE)
