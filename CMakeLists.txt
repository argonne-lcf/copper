cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME cu_fuse)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BLOCK_REDUNDANT_RPCS "Coalesce redundant RPC requests" OFF)
option(COPPER_BUILD_RUNTIME "Build the dependency-heavy Copper FUSE runtime" ON)
if (BLOCK_REDUNDANT_RPCS)
    message(STATUS "BLOCK_REDUNDANT_RPCS is enabled")
    add_definitions(-DBLOCK_REDUNDANT_RPCS=1)
else()
    message(STATUS "BLOCK_REDUNDANT_RPCS is not enabled")
endif()

if(COPPER_BUILD_RUNTIME)
set(FUSE3_SYSTEM_INCLUDE_FILE "/usr/include/fuse3/fuse.h")
set(FUSE3_REPO_INCLUDE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/frontier-fuse-headers/include/fuse.h")
set(FUSE3_INCLUDE "" CACHE PATH "Directory containing fuse.h")

if(EXISTS "${FUSE3_SYSTEM_INCLUDE_FILE}")
    message(STATUS "Using system FUSE3 headers: ${FUSE3_SYSTEM_INCLUDE_FILE}")
    set(FUSE3_INCLUDE_DIR "/usr/include/fuse3")
elseif(FUSE3_INCLUDE AND EXISTS "${FUSE3_INCLUDE}/fuse.h")
    message(STATUS "Using user-provided FUSE3 headers: ${FUSE3_INCLUDE}/fuse.h")
    set(FUSE3_INCLUDE_DIR "${FUSE3_INCLUDE}")
elseif(EXISTS "${FUSE3_REPO_INCLUDE_FILE}")
    message(STATUS "Using repo FUSE3 headers: ${FUSE3_REPO_INCLUDE_FILE}")
    set(FUSE3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts/frontier-fuse-headers/include")
else()
    message(FATAL_ERROR
        "No FUSE3 headers found. Checked ${FUSE3_SYSTEM_INCLUDE_FILE}, "
        "${FUSE3_REPO_INCLUDE_FILE}, and -DFUSE3_INCLUDE=<path>.")
endif()

set(FUSE3_SYSTEM_LIB_FILE "/usr/lib64/libfuse3.so.3")
set(FUSE3_LIB "" CACHE FILEPATH "Path to libfuse3 shared library")

if(EXISTS "${FUSE3_SYSTEM_LIB_FILE}")
    message(STATUS "Using system FUSE3 library: ${FUSE3_SYSTEM_LIB_FILE}")
    set(FUSE3_LIB_FILE "${FUSE3_SYSTEM_LIB_FILE}")
elseif(FUSE3_LIB AND EXISTS "${FUSE3_LIB}")
    message(STATUS "Using user-provided FUSE3 library: ${FUSE3_LIB}")
    set(FUSE3_LIB_FILE "${FUSE3_LIB}")
else()
    message(FATAL_ERROR
        "No FUSE3 library found. Checked ${FUSE3_SYSTEM_LIB_FILE} and "
        "-DFUSE3_LIB=<path>.")
endif()


find_package(PkgConfig REQUIRED)
find_package(MPI REQUIRED)
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
    src/metric/profiling.cpp
    src/metric/profiler_core.cpp
    src/copper/server_local_cache_provider.cpp
    src/copper/node_tree.cpp
    src/metric/ioctl_event.cpp
    src/cache/data_cache_table.cpp
    src/cache/md_cache_table.cpp
    src/cache/path_status_cache.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    "${FUSE3_INCLUDE_DIR}"
    "${MPI_INCLUDE_PATH}"
    "${MARGO_INCLUDE_DIRS}"
    "${THALLIUM_INCLUDE_DIRS}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${MPI_CXX_LIBRARIES} PkgConfig::MARGO PkgConfig::THALLIUM MPI::MPI_CXX ${FUSE3_LIB_FILE})

set(SHUTDOWN_PROJECT_NAME cu_fuse_shutdown)
set(SHUTDOWN_SOURCES src/copper/rpc_shutdown.cpp)
add_executable(${SHUTDOWN_PROJECT_NAME} ${SHUTDOWN_SOURCES})
target_link_libraries(${SHUTDOWN_PROJECT_NAME} PRIVATE PkgConfig::MARGO PkgConfig::THALLIUM ${FUSE3_LIB_FILE})

set(ADDRESSBOOK_DISCOVERY_PROJECT_NAME list_cxi_hsn_thallium)
set(ADDRESSBOOK_DISCOVERY_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/copper-tests/copper-get-addressbook/list_cxi_hsn_thallium.cpp"
)
if(EXISTS "${ADDRESSBOOK_DISCOVERY_SOURCE}")
    add_executable(${ADDRESSBOOK_DISCOVERY_PROJECT_NAME} ${ADDRESSBOOK_DISCOVERY_SOURCE})
    target_include_directories(${ADDRESSBOOK_DISCOVERY_PROJECT_NAME} PRIVATE
        "${MPI_INCLUDE_PATH}"
        "${MARGO_INCLUDE_DIRS}"
        "${THALLIUM_INCLUDE_DIRS}"
    )
    target_link_libraries(${ADDRESSBOOK_DISCOVERY_PROJECT_NAME} PRIVATE
        ${MPI_CXX_LIBRARIES}
        PkgConfig::MARGO
        PkgConfig::THALLIUM
        MPI::MPI_CXX
    )
else()
    message(WARNING
        "Optional address-book discovery source not found: ${ADDRESSBOOK_DISCOVERY_SOURCE}")
endif()

endif() # COPPER_BUILD_RUNTIME

include(CTest)
if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(test_fuse_policy tests/test_fuse_policy.cpp)
  target_include_directories(test_fuse_policy PRIVATE src)
  add_test(NAME fuse_policy COMMAND test_fuse_policy)
  add_executable(test_cache tests/test_cache.cpp)
  target_include_directories(test_cache PRIVATE src)
  target_link_libraries(test_cache PRIVATE Threads::Threads)
  add_test(NAME cache_concurrency COMMAND test_cache)
  add_executable(test_path_status_cache tests/test_path_status_cache.cpp src/cache/path_status_cache.cpp)
  target_include_directories(test_path_status_cache PRIVATE src)
  target_link_libraries(test_path_status_cache PRIVATE Threads::Threads)
  add_test(NAME path_status_cache_generations COMMAND test_path_status_cache)
  add_executable(test_profiler_core tests/test_profiler_core.cpp src/metric/profiler_core.cpp)
  target_include_directories(test_profiler_core PRIVATE src)
  target_link_libraries(test_profiler_core PRIVATE Threads::Threads)
  add_test(NAME profiler_core COMMAND test_profiler_core)
  add_executable(test_operations_config tests/test_operations_config.cpp src/metric/operations.cpp src/metric/profiler_core.cpp)
  target_include_directories(test_operations_config PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/scripts/frontier-fuse-headers/include")
  target_link_libraries(test_operations_config PRIVATE Threads::Threads)
  add_test(NAME operations_config COMMAND test_operations_config)
  add_executable(test_failure_status tests/test_failure_status.cpp)
  target_include_directories(test_failure_status PRIVATE src)
  add_test(NAME failure_status COMMAND test_failure_status)
  add_executable(test_profiling_disabled tests/test_profiling_disabled.cpp src/metric/profiling.cpp src/metric/operations.cpp src/metric/profiler_core.cpp)
  target_include_directories(test_profiling_disabled PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/scripts/frontier-fuse-headers/include")
  target_link_libraries(test_profiling_disabled PRIVATE Threads::Threads)
  add_test(NAME profiling_disabled COMMAND test_profiling_disabled)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME aggregate_profiling COMMAND ${Python3_EXECUTABLE} -m unittest tests/test_aggregate_profiling.py -v)
    add_test(NAME production_profiler_wiring COMMAND ${Python3_EXECUTABLE} -m unittest tests/test_production_profiler_wiring.py -v)
    add_test(NAME launch_copper_aurora COMMAND ${Python3_EXECUTABLE} -m unittest tests/test_launch_copper_aurora.py -v)
    set_tests_properties(aggregate_profiling production_profiler_wiring launch_copper_aurora PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
endif()


if(COPPER_BUILD_RUNTIME)
# After building cu_fuse, copy the scripts into the build directory
# and make them executable for everyone.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/launch_copper_aurora.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/launch_copper_aurora.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/launch_copper_frontier.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/launch_copper_frontier.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stop_copper_aurora.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/stop_copper_aurora.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stop_copper_frontier.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/stop_copper_frontier.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/aggregate_profiling.py"
        "${CMAKE_CURRENT_BINARY_DIR}/aggregate_profiling.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/olcf_frontier_copper_addressbook.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/olcf_frontier_copper_addressbook.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/alcf_aurora_copper_addressbook.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/alcf_aurora_copper_addressbook.txt"
    COMMAND chmod a+rx
        "${CMAKE_CURRENT_BINARY_DIR}/launch_copper_aurora.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/launch_copper_frontier.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/stop_copper_aurora.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/stop_copper_frontier.sh"
        "${CMAKE_CURRENT_BINARY_DIR}/aggregate_profiling.py"
    COMMENT "Copying cu_fuse runtime scripts and address-book files to the build directory"
    VERBATIM
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
endif() # COPPER_BUILD_RUNTIME
