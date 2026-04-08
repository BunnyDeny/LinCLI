cmake_minimum_required(VERSION 3.10)
project(cli_core)

set(INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
)
set(SUBDIRS Src)

foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()

add_library(cli_core STATIC ${SOURCES})

target_include_directories(cli_core PUBLIC
    ${INCLUDE_DIRS}
)

target_link_libraries(cli_core PUBLIC cli_lib)
