// Minimal Thallium server example that returns a string payload sized by the
// client request.

#include <iostream>
#include <thallium.hpp>
#include <chrono>
#include <thallium/serialization/stl/string.hpp>

namespace tl = thallium;

void sum(const tl::request& req, const int req_bytes) {
    std::string req_from_addr = static_cast<std::string>(req.get_endpoint());

    std::chrono::time_point<std::chrono::system_clock> start1, end1;
    start1 = std::chrono::system_clock::now();
    
    std::string file_content(req_bytes, 'c');

    end1 = std::chrono::system_clock::now();           
    std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
    std::cout << "For requester " << req_from_addr 
              << " Root Buffer creation time " << elapsed_seconds1.count() << " s " 
              << " Bytes " << req_bytes << std::endl ; 

    req.respond(file_content);
}

int main(int argc, char** argv) {
    auto serverEngine = tl::engine{"cxi", THALLIUM_SERVER_MODE};
    std::cout << "Server running at address " << serverEngine.self() << std::endl;
    serverEngine.define("sum", sum);
    serverEngine.wait_for_finalize();
    return 0;
}
