cmake_minimum_required(VERSION 3.6)

project(copper_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(MPI REQUIRED)
pkg_check_modules(MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules(THALLIUM REQUIRED IMPORTED_TARGET thallium)

include_directories(${MPI_INCLUDE_PATH})
include_directories(${MARGO_INCLUDE_DIRS})
include_directories(${THALLIUM_INCLUDE_DIRS})

find_package(thallium CONFIG REQUIRED)
find_package(mercury CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)

function(add_copper_test target source output_name)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${MPI_INCLUDE_PATH}"
        "${MARGO_INCLUDE_DIRS}"
        "${THALLIUM_INCLUDE_DIRS}"
    )
    target_link_libraries(${target} PRIVATE
        ${MPI_CXX_LIBRARIES}
        PkgConfig::MARGO
        PkgConfig::THALLIUM
        MPI::MPI_CXX
    )
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${output_name}")
endfunction()

# Small RPC and shutdown experiments.
add_copper_test(test1_single_rpc test1/copper_special_single_rpc.cpp copper_special_single_rpc)
add_copper_test(test1_shutdown test1/shutdown.cpp shutdown)
add_copper_test(test1_shutdown_special test1/shutdown_special.cpp shutdown_special)

# Tree-forwarding experiment.
add_copper_test(test2_tree_rpc test2/copper.cpp copper)

# Minimal RPC latency/client-server examples.
add_copper_test(test3_client_rpc test3-rpc/client_rpc.cpp client_rpc)
add_copper_test(test3_server_rpc test3-rpc/server_rpc.cpp server_rpc)

# Minimal bulk RDMA examples.
add_copper_test(test4_client_bulk_rdma test4-rdma/client_bulk_rdma.cpp client_bulk_rdma)
add_copper_test(test4_server_bulk_rdma test4-rdma/server_bulk_rdma.cpp server_bulk_rdma)

# Address-book discovery helper used by version9 discover mode.
add_copper_test(test5_list_cxi_hsn_thallium
    copper-get-addressbook/list_cxi_hsn_thallium.cpp
    list_cxi_hsn_thallium
)

install(TARGETS
    test1_single_rpc
    test1_shutdown
    test1_shutdown_special
    test2_tree_rpc
    test3_client_rpc
    test3_server_rpc
    test4_client_bulk_rdma
    test4_server_bulk_rdma
    test5_list_cxi_hsn_thallium
    DESTINATION bin
)
