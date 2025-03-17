#include <iostream>
#include <thallium.hpp>
#include <chrono>
#include <thallium/serialization/stl/string.hpp>

namespace tl = thallium;

int main(int argc, char** argv) {

    std::string server_addr = argv[1];
    int req_bytes = atoi(argv[2]);
    auto clientEngine = tl::engine{"cxi", THALLIUM_CLIENT_MODE};

    tl::remote_procedure sum = clientEngine.define("sum");
    tl::endpoint server = clientEngine.lookup(server_addr);

    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    std::string client_file_content  = sum.on(server)(req_bytes);
    end = std::chrono::system_clock::now();           
    std::chrono::duration<double> elapsed_seconds = end - start;
    
    std::cout << "I'm the main requester " << clientEngine.self() 
              << "responded by "     << server 
              << " elapsed time "    << elapsed_seconds.count() << " s " 
              << " Bytes "           << req_bytes << std::endl ; 
    std::cout << " Server answered " << client_file_content << std::endl;

    return 0;
}
