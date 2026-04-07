cmake_minimum_required(VERSION 3.10)
project(cli_core)

# Find all source files in Src directory
file(GLOB SOURCES "Src/*.c")

# Create static library
add_library(cli_core STATIC ${SOURCES})

# Include directories
target_include_directories(cli_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
)

# Link the library
target_link_libraries(cli_core PUBLIC cli_lib)
