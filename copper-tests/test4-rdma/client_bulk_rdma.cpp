#include <iostream>
#include <thallium.hpp>
#include <chrono>

namespace tl = thallium;

int main(int argc, char** argv) {
    std::string server_addr = argv[1];
    int req_bytes = atoi(argv[2]);

    auto clientEngine = tl::engine{"cxi://cxi0:1", THALLIUM_CLIENT_MODE};
    tl::remote_procedure remote_do_rdma = clientEngine.define("do_rdma");
    tl::endpoint server_endpoint = clientEngine.lookup(server_addr);

    std::chrono::time_point<std::chrono::system_clock> start1, end1;
    start1 = std::chrono::system_clock::now();
    
    char* buffer = new char[req_bytes];
    for (int i = 0; i < req_bytes; i++) 
    {
        buffer[i] = 'c';
    }
    std::string file_content = buffer;

    std::vector<std::pair<void*,std::size_t>> segments(1);
    segments[0].first  = (void*)(&file_content[0]);
    segments[0].second = file_content.size()+1;

    tl::bulk myBulk = clientEngine.expose(segments, tl::bulk_mode::read_only);

    end1 = std::chrono::system_clock::now();           
    std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
    std::cout << "For requester " << server_endpoint 
              << " Root Buffer creation time " << elapsed_seconds1.count() << " s " 
              << " Bytes " << req_bytes << std::endl ; 

    remote_do_rdma.on(server_endpoint)(myBulk);

    return 0;
}
