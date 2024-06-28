cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME cu_fuse)
project(${PROJECT_NAME})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

set(EXTERNAL_LIBS
  "${CMAKE_SOURCE_DIR}/external/fuse3/install/lib/x86_64-linux-gnu/libfuse3.so"
)

set(EXTERNAL_HEADERS
  "${CMAKE_SOURCE_DIR}/external/fuse3/install/include/fuse3"
)

# Add your source files
set(SOURCES
    src/entry.cpp
    src/fs/cu_stat.cpp
    src/fs/util.cpp
    src/cache/tree_cache_table.cpp
    src/metric/operations.cpp
    src/metric/cache_event.cpp
    src/metric/metrics.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include FUSE headers
target_include_directories(${PROJECT_NAME} PRIVATE "${EXTERNAL_HEADERS}")

# Link FUSE libraries
target_link_libraries(${PROJECT_NAME} PRIVATE ${EXTERNAL_LIBS})
