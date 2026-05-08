cmake_minimum_required(VERSION 3.10)

# ============================================================
# Demo commands object library
# ============================================================
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/lib
)
set(SUBDIRS commands)
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

# ============================================================
# Unity unit tests executable
# ============================================================
set(UNIT_TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
)
set(UNIT_TEST_SUBDIRS unit unity)

set(UNIT_TEST_SOURCES "")
foreach(dir ${UNIT_TEST_SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND UNIT_TEST_SOURCES ${TMP})
endforeach()

add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE src)
target_include_directories(unit_tests PUBLIC
    ${UNIT_TEST_INCLUDE_DIRS}
)
