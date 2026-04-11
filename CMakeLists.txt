cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")
set(OUTPUT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(MODULES lib cli init)

foreach(mod ${MODULES})
    add_subdirectory(${mod})
endforeach()

# 直接链接 object 文件而不是库
add_executable(${OUTPUT_NAME}
    $<TARGET_OBJECTS:lib>
    $<TARGET_OBJECTS:cli>
    $<TARGET_OBJECTS:init>
)
target_include_directories(${OUTPUT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)
target_link_libraries(${OUTPUT_NAME} PRIVATE m)
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)
