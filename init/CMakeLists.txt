cmake_minimum_required(VERSION 3.10)

set(INCLUDE_DIRS 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)

set(SUBDIRS .)

set(PRO_NAME ${OUTPUT_NAME})

set(DENPS cli_core cli_lib)

project(${PRO_NAME})

foreach(dir ${SUBDIRS})
    file(GLOB_RECURSE TMP "${dir}/*.c")
    list(APPEND SOURCES ${TMP})
endforeach()

add_executable(${PRO_NAME} ${SOURCES})

target_include_directories(${PRO_NAME} PUBLIC
    ${INCLUDE_DIRS}
)

target_link_libraries(${PRO_NAME} PUBLIC cli_core cli_lib)

set_target_properties(${PRO_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)



# cmake_minimum_required(VERSION 3.10)

# set(INCLUDE_DIRS 
#     ${CMAKE_SOURCE_DIR}/include
#     ${CMAKE_SOURCE_DIR}/cli/Inc
# )

# set(SUBDIRS .)

# set(PRO_NAME cli_init)

# set(DENPS cli_core cli_lib)

# project(${PRO_NAME})

# foreach(dir ${SUBDIRS})
#     file(GLOB_RECURSE TMP "${dir}/*.c")
#     list(APPEND SOURCES ${TMP})
# endforeach()

# add_executable(${PRO_NAME} ${SOURCES})

# target_include_directories(${PRO_NAME} PUBLIC
#     ${INCLUDE_DIRS}
# )

# target_link_libraries(${PRO_NAME} PUBLIC ${DENPS})

# set_target_properties(${PRO_NAME} PROPERTIES
#     LINK_FLAGS "-T ${LINKER_SCRIPT}"
# )

