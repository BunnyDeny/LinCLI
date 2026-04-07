cmake_minimum_required(VERSION 3.10)
project(cli_project)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 定义可执行文件名称和链接脚本
set(CLI_INIT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(init)
