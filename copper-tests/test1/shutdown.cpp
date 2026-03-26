// Read a local address-book file and request remote shutdown on each endpoint.

#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <mpi.h>
#include <array>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

namespace tl = thallium;

int main(int argc, char** argv) 
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sleep_seconds>" << std::endl;
        return 1;
    }

    sleep(atoi(argv[1]));  

    auto engine = tl::engine{"cxi://cxi0:1", THALLIUM_SERVER_MODE};
    std::cout << "shutdown Server running at address " << engine.self() << std::endl;

    std::ifstream inFile("./copper_address_book.txt", std::ios::in);
    if (!inFile.is_open()) 
    {
        std::cout  << "Error opening file" << std::endl;
        return 0;
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
