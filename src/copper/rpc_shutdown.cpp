#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <mpi.h>
#include <array>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

namespace tl = thallium;

#define USAGE "./cu_fuse_shutdown <SLEEP_TIME> <COPPER_LOGS_DIR>"
#define COPPER_ADDRESS_BOOK_FILE_NAME "copper_address_book.txt"

int main(int argc, char** argv) {
    if(argc <= 2) {
        std::cerr << "usage: " << USAGE << std::endl;
        return -1;
    }

    const std::string copper_logs_dir = argv[2];
    const std::string address_book_abs_path = copper_logs_dir + "/" + COPPER_ADDRESS_BOOK_FILE_NAME;
    auto sleep_time = atoi(argv[1]);

    std::cout << "found copper_logs_dir: " << copper_logs_dir << std::endl;
    std::cout << "copper address book absolute path: " << address_book_abs_path << std::endl;
    std::cout << "sleeping for " << sleep_time << " seconds" << std::endl;
    sleep(sleep_time);

    auto engine = tl::engine{"cxi://cxi0:1", THALLIUM_SERVER_MODE};
    std::cout << "shutdown Server running at address " << engine.self() << std::endl;

    std::ifstream inFile(address_book_abs_path, std::ios::in);
    if (!inFile.is_open()) 
    {
        std::cout  << "Error opening file" << std::endl;
        return -1;
    }
    std::string line;
    while (getline(inFile, line))   
    {
        size_t pos = line.find(" ");
        std::string first_part_hostname = line.substr(0, pos);
        std::string second_part_cxi     = line.substr(pos + 1);

        std::cout  << "Calling shutdown_remote_engine on " << second_part_cxi << std::endl;
        auto local_server_endpoint = engine.lookup(second_part_cxi);      
        engine.shutdown_remote_engine(local_server_endpoint);
    }

    inFile.close();

    // Finalize this server.
    engine.finalize();

    return 0;
}
