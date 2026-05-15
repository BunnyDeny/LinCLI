cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/tests/unity
    ${CMAKE_CURRENT_SOURCE_DIR}
)
set(SUBDIRS .)
file(GLOB_RECURSE EXTRA_SOURCES "${CMAKE_SOURCE_DIR}/tests/unity/*.c")
include(${CMAKE_SOURCE_DIR}/cmake/lincli_build.cmake)
lincli_build_executable()
target_link_libraries(${PRO_NAME} PRIVATE src commands)
