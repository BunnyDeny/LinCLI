cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)
set(SUBDIRS .)
set(PRO_NAME cli_init)

project(${PRO_NAME})
foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()
add_library(${PRO_NAME} OBJECT ${SOURCES})
target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)
