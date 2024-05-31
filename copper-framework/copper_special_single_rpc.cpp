// module use /soft/preview-modulefiles/24.086.0
// module load frameworks/2024.04.15.002
// . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
// spack env activate kaushik_env_1 
// make file=copper_special_single_rpc
// ./copper_special_single_rpc  cxi://cxi0:1 ofi+cxi://0x20110400

#include <iostream>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h> 
#include <cmath>
#include <mutex>
#include <sstream>
#include <bitset>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <thread>
#include <queue>
#include <chrono>
#include <ctime>

namespace tl = thallium;

class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> 
{
    std::vector<tl::endpoint> m_peers;
    tl::remote_procedure      m_get_file_data;
    public:
        ServerLocalCacheProvider(tl::engine serverEngine) : tl::provider<ServerLocalCacheProvider>{std::move(serverEngine), 0} , m_get_file_data{define("get_file_data", &ServerLocalCacheProvider::getFileData)}
        {
            get_engine().push_finalize_callback([this](){ delete this;});
        }

        void getFileData(const tl::request& req, const std::string& filename, const int req_bytes) 
        {
            std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
            std::cout << "req_coming_from_addr " << req_from_addr  << " requested data for file : " << filename << " requested bytes : " << req_bytes << std::endl;

            std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

            std::chrono::time_point<std::chrono::system_clock> start1, end1;
            start1 = std::chrono::system_clock::now();
            
            char* buffer = new char[req_bytes];
            for (int i = 0; i < req_bytes; i++) 
            {
                buffer[i] = 'c';
            }
            std::string file_content =buffer;

            end1 = std::chrono::system_clock::now();           
            std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
            std::cout << "For requester " << req_from_addr << " Root Buffer creation time " << elapsed_seconds1.count() << " s " << " Bytes " << req_bytes << std::endl ; 

            req.respond(file_content);

            // margo_instance_id mid;
            // mid  = get_engine().get_margo_instance();
            // char* state_file_name;
            // margo_state_dump(mid, "margo-example-server", 1, &state_file_name);
            // printf("# Runtime state dumped to %s\n", state_file_name);
            // free(state_file_name);

        }


        ServerLocalCacheProvider(const ServerLocalCacheProvider&)              = delete;
        ServerLocalCacheProvider(ServerLocalCacheProvider&&)                   = delete;
        ServerLocalCacheProvider& operator = (const ServerLocalCacheProvider&) = delete;
        ServerLocalCacheProvider& operator = (ServerLocalCacheProvider&&)      = delete;

};



int main(int argc, char** argv) 
{
        std::string my_addr = argv[1];
        std::string remote_addr = argv[2];
    
        auto serverEngine = tl::engine{my_addr, THALLIUM_SERVER_MODE};
        std::cout << "Server running at address " << serverEngine.self() << std::endl;
        // serverEngine.enable_remote_shutdown();
        auto get_file_data = serverEngine.define("get_file_data");
        new ServerLocalCacheProvider{serverEngine};
        sleep(2); //  barrier issue: all process need to wait until the server is created.
        auto remote_server_endpoint = serverEngine.lookup(remote_addr);      


        // std::cout  <<"Main pid " << getpid() << " " << hostname<< std::endl;
        // {
        //         // thallium::xstream primary = thallium::xstream::self();
        //         // primary.make_thread() ;

        //         auto worker1 = tl::xstream::self().make_thread([serverEngine, get_file_data]() 
        //                 { /* thread function here... */     
        //                     std::thread::id this_tid = std::this_thread::get_id();
        //                     std::cout  <<"Hello from dummy_fuse_func with pid " << getpid()   <<" thread id " << this_tid  << std::endl;
        //                     std::string thread_file_content = get_file_data.on(remote_server_endpoint)(std::string{"/path/to/file"}, 1);  
        //                     std::cout << "From thread received content : "  << thread_file_content << std::endl;
        //                 });
        // }

        {

            std::chrono::time_point<std::chrono::system_clock> start, end;

            start = std::chrono::system_clock::now();
            std::string client_file_content = get_file_data.on(remote_server_endpoint)(std::string{"/path/to/file"}, 2);  
            end = std::chrono::system_clock::now();           
            
            std::chrono::duration<double> elapsed_seconds = end - start;
            std::cout << "I'm the main requester " << serverEngine.self() 
                      << " elapsed time " << elapsed_seconds.count() << " s " << " Bytes " << 2 << std::endl ; 
            // std::cout << "Finally received content : "  << client_file_content << std::endl;
        }


        // serverEngine.wait_for_finalize();
        serverEngine.finalize();


    return 0;
}
