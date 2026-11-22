set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

get_directory_property(_defs COMPILE_DEFINITIONS)
list(FILTER _defs EXCLUDE REGEX "^LOG_PREFIX=")
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${_defs}")
add_compile_definitions(LOG_PREFIX="coldtrace-worker: ")

add_executable(coldtrace-worker
    main.c
    manager.c
    consumer.c)
target_link_libraries(coldtrace-worker PRIVATE dice.h lz4 Threads::Threads)
