cmake_minimum_required(VERSION 3.10)

# list 是纯头文件库，所有代码在 list.h 中
add_library(list INTERFACE)
target_include_directories(list INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/list)

# workqueue 子目录
add_subdirectory(workqueue)

# hexdump 子目录
add_subdirectory(hexdump)
