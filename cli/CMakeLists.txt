cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
)
set(SUBDIRS Src)
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
