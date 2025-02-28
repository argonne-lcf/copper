// module use /soft/preview-modulefiles/24.086.0
// module load frameworks/2024.04.15.002
// . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
// spack env activate kaushik_env_1 
// make file=shutdown
// ./shutdown 2


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
