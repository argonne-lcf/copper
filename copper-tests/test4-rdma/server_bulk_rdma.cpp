#include <iostream>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <chrono>

namespace tl = thallium;

int main(int argc, char** argv) {
    std::string my_addr = argv[1];
    static int req_bytes = atoi(argv[2]);

    auto serverEngine = tl::engine{my_addr, THALLIUM_SERVER_MODE};

    std::function<void(const tl::request&, tl::bulk&)> f =
        [&serverEngine](const tl::request& req, tl::bulk& remote_bulk) {
            tl::endpoint client_ep = req.get_endpoint();
            std::vector<char> received_vector(req_bytes);
            std::vector<std::pair<void*,std::size_t>> segments(1);
            segments[0].first  = (void*)(&received_vector[0]);
            segments[0].second = received_vector.size();
            tl::bulk local = serverEngine.expose(segments, tl::bulk_mode::write_only);

            std::chrono::time_point<std::chrono::system_clock> start1, end1;
            start1 = std::chrono::system_clock::now();
            remote_bulk.on(client_ep) >> local;
            end1 = std::chrono::system_clock::now();           
            std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
            std::cout << "I'm the main requester server " << serverEngine.self() 
            << " Receiving from client " << client_ep 
            << " RDMA transfer time " << elapsed_seconds1.count() << " s " 
            << " Bytes "           << req_bytes << std::endl ; 

            std::cout << "Server received bulk: ";
            for(auto c : received_vector) std::cout << c;
            std::cout << std::endl;
            req.respond();
        };
    serverEngine.define("do_rdma",f);

    serverEngine.wait_for_finalize();
}