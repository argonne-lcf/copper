// module use /soft/preview-modulefiles/24.086.0
// module load frameworks/2024.04.15.002
// . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
// spack env activate kaushik_env_1 
// make
// mkdir /tmp/my_mnt_dir
// fusermount -uz  /tmp/my_mnt_dir 
// killall -9 copper
// ./copper -f /tmp/my_mnt_dir



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
extern tl::remote_procedure get_file_data;
int cu_hello_main(int argc, char *argv[], void* userdata); 

class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> 
{
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
 
                char* buffer = new char[req_bytes];
                for (int i = 0; i < req_bytes; i++) 
                {
                    buffer[i] = 'c';
                }
                std::string file_content =buffer;
                req.respond(file_content);
        }

        ServerLocalCacheProvider(const ServerLocalCacheProvider&)              = delete;
        ServerLocalCacheProvider(ServerLocalCacheProvider&&)                   = delete;
        ServerLocalCacheProvider& operator = (const ServerLocalCacheProvider&) = delete;
        ServerLocalCacheProvider& operator = (ServerLocalCacheProvider&&)      = delete;
};

int main(int argc, char** argv) 
{ 
        auto serverEngine = tl::engine{"cxi", THALLIUM_SERVER_MODE, 1, -1 };
        get_file_data = serverEngine.define("get_file_data");

        new ServerLocalCacheProvider{serverEngine};
        serverEngine.enable_remote_shutdown();


        std::cout << "Server running at address " << serverEngine.self() << std::endl;

        sleep(2); //  barrier issue: all process need to wait until the server is created.


        
        std::cout  <<"Main pid " << getpid() << " " << std::endl;
        pthread_t tid;
        tid = pthread_self();
        std::cout<<tid<<std::endl;

        // std::thread worker1(dummy_fuse_func, argc, argv);
        // worker1.join();
        // dummy_fuse_func( argc, argv);
        cu_hello_main(argc, argv, &serverEngine);


        {
            std::chrono::time_point<std::chrono::system_clock> start, end;

            start = std::chrono::system_clock::now();
            // std::string client_file_content = get_file_data.on(serverEngine.self())(std::string{"/path/to/file"}, atoi(argv[1]));  
            std::string client_file_content = get_file_data.on(serverEngine.self())(std::string{"/path/to/file"}, 4);  

            end = std::chrono::system_clock::now();           
            
            std::chrono::duration<double> elapsed_seconds = end - start;
            std::cout << "I'm the main requester " 
                    //   << " elapsed time " << elapsed_seconds.count() << " s " << " Bytes " << atoi(argv[1]) << std::endl ; 
                         << " elapsed time " << elapsed_seconds.count() << " s " << " Bytes " << 4 << std::endl ; 

            std::cout << "Finally received content : "  << client_file_content << std::endl;
        }

        serverEngine.wait_for_finalize();

    return 0;
}
