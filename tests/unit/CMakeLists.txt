cmake_minimum_required(VERSION 3.10)
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/tests/unity
    ${CMAKE_CURRENT_SOURCE_DIR}
)
set(SUBDIRS .)
set(PRO_NAME "unit_tests")

project(${PRO_NAME})
foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()

file(GLOB_RECURSE UNITY_SOURCES "${CMAKE_SOURCE_DIR}/tests/unity/*.c")
list(APPEND SOURCES ${UNITY_SOURCES})

add_executable(${PRO_NAME} ${SOURCES})
target_link_libraries(${PRO_NAME} PRIVATE src commands)
target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)
