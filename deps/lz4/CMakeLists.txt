add_library(lz4_headers INTERFACE)
target_include_directories(lz4_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(lz4 STATIC lz4.c)
target_link_libraries(lz4 PUBLIC lz4_headers)

target_compile_options(lz4 PRIVATE -w)
