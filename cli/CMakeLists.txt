cmake_minimum_required(VERSION 3.10)

set(INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
)

set(SUBDIRS Src)

set(PRO_NAME cli_core)

set(DENPS cli_lib)

project(${PRO_NAME})

foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()

add_library(${PRO_NAME} STATIC ${SOURCES})

target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)

target_link_libraries(${PRO_NAME} PUBLIC ${DENPS})
