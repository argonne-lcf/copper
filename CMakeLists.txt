cmake_minimum_required(VERSION 3.6)

set(PROJECT_NAME cu_fuse)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (BLOCK_REDUNDANT_RPCS)
    message(STATUS "BLOCK_REDUNDANT_RPCS is enabled")
    add_definitions(-DBLOCK_REDUNDANT_RPCS=1)
else()
    message(STATUS "BLOCK_REDUNDANT_RPCS is not enabled")
endif()

set(FUSE3_INCLUDE_FILE "/usr/include/fuse3/fuse.h")
if(EXISTS "${FUSE3_INCLUDE_FILE}")
    message(STATUS "File ${FUSE3_INCLUDE_FILE} exists.")
    set(FUSE3_INCLUDE_DIR "/usr/include/fuse3")
else()
    message(WARNING "Required File ${FUSE3_INCLUDE_FILE} does not exist. Please set -DFUSE3_INCLUDE=<path> when invoking CMake.")
endif()

set(FUSE3_LIB_FILE "/usr/lib64/libfuse3.so")
if(EXISTS "${FUSE3_LIB_FILE}")
    message(STATUS "File ${FUSE3_LIB_FILE} exists.")
    set(FUSE3_LIB_DIR "/usr/lib64")
else()
    message(WARNING "Required File ${FUSE3_LIB_FILE} does not exist. Please set -DFUSE3_LIB=<path> when invoking CMake.")
endif()

link_directories(${FUSE3_LIB_DIR})

find_package(PkgConfig REQUIRED)
find_package(MPI REQUIRED)
set(CMAKE_CXX_COMPILER mpicxx)
pkg_check_modules (MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules (THALLIUM REQUIRED IMPORTED_TARGET thallium)

include_directories(${MPI_INCLUDE_PATH})
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
    src/cache/path_status_cache.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${FUSE3_INCLUDE_DIR}" "${MPI_INCLUDE_PATH}" "${MARGO_INCLUDE_DIRS}" "${THALLIUM_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${MPI_CXX_LIBRARIES} PkgConfig::MARGO PkgConfig::THALLIUM fuse3 MPI::MPI_CXX)

set(SHUTDOWN_PROJECT_NAME cu_fuse_shutdown)
set(SHUTDOWN_SOURCES src/copper/rpc_shutdown.cpp)
add_executable(${SHUTDOWN_PROJECT_NAME} ${SHUTDOWN_SOURCES})
target_link_libraries(${SHUTDOWN_PROJECT_NAME} PRIVATE PkgConfig::MARGO PkgConfig::THALLIUM)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(PROGRAMS scripts/launch_copper.sh DESTINATION bin)
