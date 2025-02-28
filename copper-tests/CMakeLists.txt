cmake_minimum_required(VERSION 3.6)

set(PROJECT_NAME copper_rpc_tests)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set( SOURCES test1/copper_special_single_rpc.cpp )
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${MPI_INCLUDE_PATH}" "${MARGO_INCLUDE_DIRS}" "${THALLIUM_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${MPI_CXX_LIBRARIES} PkgConfig::MARGO PkgConfig::THALLIUM MPI::MPI_CXX)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(PROGRAMS scripts/launch_copper.sh DESTINATION bin)
install(PROGRAMS scripts/stop_copper.sh DESTINATION bin)
