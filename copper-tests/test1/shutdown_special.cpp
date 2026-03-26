// Send one explicit remote shutdown request to a single target endpoint.

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
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <my_addr> <remote_addr>" << std::endl;
        return 1;
    }

    std::string my_addr = argv[1];
    std::string remote_addr = argv[2];

    auto engine = tl::engine{my_addr, THALLIUM_SERVER_MODE};
    std::cout << "shutdown Server running at address " << engine.self() << std::endl;
    std::cout  << "Calling shutdown_remote_engine on " << remote_addr << std::endl;
    auto remote_server_endpoint = engine.lookup(remote_addr);      
    engine.shutdown_remote_engine(remote_server_endpoint);
    engine.finalize();
    return 0;
}
