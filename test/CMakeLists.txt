# -Wno-error=unused-command-line-argument

file(GLOB SRCS *.c *.cpp)
foreach(SRC ${SRCS})
  get_filename_component(TARGET ${SRC} NAME_WLE)
  add_executable(${TARGET} ${SRC})
  target_compile_options(${TARGET} PRIVATE -fsanitize=thread)
  target_link_libraries(${TARGET} PRIVATE pthread vsync tsan)
endforeach()
