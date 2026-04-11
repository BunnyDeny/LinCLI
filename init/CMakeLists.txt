cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)
set(SUBDIRS .)
set(PRO_NAME cli_init)

project(${PRO_NAME})
# 显式排除 main.c
set(SOURCES "")
foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    foreach(src ${TMP})
        if(NOT src MATCHES "main\\.c$")
            list(APPEND SOURCES ${src})
        endif()
    endforeach()
endforeach()
add_library(${PRO_NAME} OBJECT ${SOURCES})
target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)
