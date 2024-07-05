cmake_minimum_required(VERSION 3.6)

set(PROJECT_NAME cu_fuse)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER mpicxx)

if(NOT DEFINED FUSE3_INCLUDE)
    message(FATAL_ERROR "Required variable FUSE3_INCLUDE is not defined. Please set -DFUSE3_INCLUDE=<path> when invoking CMake.")
endif()

if(NOT DEFINED FUSE3_LIB)
    message(FATAL_ERROR "Required variable FUSE3_LIB is not defined. Please set -DFUSE3_LIB=<path> when invoking CMake.")
endif()

link_directories(${FUSE3_LIB})

find_package(PkgConfig REQUIRED)

pkg_check_modules (MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules (THALLIUM REQUIRED IMPORTED_TARGET thallium)

include_directories(${MARGO_INCLUDE_DIRS})
include_directories(${THALLIUM_INCLUDE_DIRS})

find_package(thallium CONFIG REQUIRED)
find_package(mercury CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)

get_target_property (CEREAL_INC cereal::cereal INTERFACE_INCLUDE_DIRECTORIES)

set(SOURCES
    src/entry.cpp
    src/fs/cu_fuse.cpp
    src/fs/cu_stat.cpp
    src/fs/util.cpp
    src/cache/tree_cache_table.cpp
    src/metric/operations.cpp
    src/metric/cache_event.cpp
    src/metric/metrics.cpp
    src/copper/server_local_cache_provider.cpp
    src/copper/node_tree.cpp
    src/metric/ioctl_event.cpp
    src/cache/data_cache_table.cpp
    src/cache/md_cache_table.cpp
    src/cache/md_cache_table.cpp
    src/cache/path_status_cache.cpp
    src/cache/path_status_cache.h
    src/cache/path_status_cache.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${FUSE3_INCLUDE}")
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::MARGO PkgConfig::THALLIUM fuse3)
