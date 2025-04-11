cmake_minimum_required(VERSION 3.20)
project(deps)
include(ExternalProject)

# configure and install libvsync in build/deps
execute_process(
  COMMAND
    ${CMAKE_COMMAND} #
    -S${CMAKE_CURRENT_SOURCE_DIR}/libvsync-installer
    -B${CMAKE_CURRENT_BINARY_DIR}/libvsync-installer
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

execute_process(COMMAND ${CMAKE_COMMAND} --build
                        ${CMAKE_CURRENT_BINARY_DIR}/libvsync-installer)

# configure and install bingo in build/deps
execute_process(
  COMMAND
    ${CMAKE_COMMAND} #
    -S${CMAKE_CURRENT_SOURCE_DIR}/bingo-installer
    -B${CMAKE_CURRENT_BINARY_DIR}/bingo-installer
    -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_FIND_ROOT_PATH=${CMAKE_CURRENT_BINARY_DIR})

execute_process(COMMAND ${CMAKE_COMMAND} --build
                        ${CMAKE_CURRENT_BINARY_DIR}/bingo-installer)
