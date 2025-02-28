// module use /soft/preview-modulefiles/24.086.0
// module load frameworks/2024.04.15.002
// . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
// spack env activate kaushik_env_1 
// make file=shutdown_special
// ./shutdown  cxi://cxi0:1 ofi+cxi://0x20110400

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
