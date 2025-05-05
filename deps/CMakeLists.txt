include(ExternalProject)

# configure and install libvsync in build/deps
execute_process(
  COMMAND
    ${CMAKE_COMMAND} #
    -S${CMAKE_CURRENT_SOURCE_DIR}/libvsync-installer
    -B${CMAKE_CURRENT_BINARY_DIR}/libvsync-installer
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

execute_process(COMMAND ${CMAKE_COMMAND} --build
                        ${CMAKE_CURRENT_BINARY_DIR}/libvsync-installer)
