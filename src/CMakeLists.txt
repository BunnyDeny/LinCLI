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
get_filename_component(PRO_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PRO_NAME})
foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()
add_library(${PRO_NAME} OBJECT ${SOURCES})
target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)
