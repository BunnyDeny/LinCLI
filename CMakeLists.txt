cmake_minimum_required(VERSION 3.10)
project(cli_project)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置全局编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# 定义可执行文件名称
set(OUTPUT_NAME "a.out")

# 指定链接脚本
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")

# 设置可执行文件目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置子CMakeLists.txt所在文件夹
set(MODULES lib cli init)

# 执行子CMakeLists.txt
foreach(mod ${MODULES})
    add_subdirectory(${mod})
endforeach()
