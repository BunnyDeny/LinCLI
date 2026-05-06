cmake_minimum_required(VERSION 3.10)

# ============================================================
# Unity 单元测试
# ============================================================
set(UNIT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_atoi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_mpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_vector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_vsnprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_statem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_errno.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_runners.c
    ${CMAKE_SOURCE_DIR}/init/critical_default.c
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE lincli)
target_include_directories(unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/cli/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
)

# ============================================================
# 原有交互式测试命令（演示示例）
# ============================================================
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/cli/Inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
)
set(SUBDIRS .)
get_filename_component(PRO_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PRO_NAME})
foreach(dir ${SUBDIRS})
    file(GLOB TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()
add_library(${PRO_NAME} OBJECT ${SOURCES})
target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)
