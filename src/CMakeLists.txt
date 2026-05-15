cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/init
    ${CMAKE_SOURCE_DIR}/src/third_party/list
    ${CMAKE_SOURCE_DIR}/src/third_party/workqueue
    ${CMAKE_SOURCE_DIR}/src/third_party/hexdump
)
set(SUBDIRS cli lib init third_party/workqueue third_party/hexdump)
include(${CMAKE_SOURCE_DIR}/cmake/lincli_build.cmake)
lincli_build_object()
