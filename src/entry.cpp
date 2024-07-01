#include "fs/util.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <thread>
#include <unistd.h>
#include <vector>

#include "copper/node_tree.h"
#include "copper/server_local_cache_provider.h"
#include "fs/cu_fuse.h"

int main(int argc, char** argv) {
    std::cout << "1" << std::endl;
    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);
    std::string curr_node_addr_server;
    std::string curr_node_addr_client;

    ServerLocalCacheProvider::my_hostname = std::string(char_hostname) + "-" + std::to_string(getpid()) + ".log";
    std::cout << "attempting write to" << ServerLocalCacheProvider::my_hostname << std::endl;

    auto serverEngine = tl::engine{"na+sm", THALLIUM_SERVER_MODE, 1, -1};
    std::cout << "writing address to address book: " << serverEngine.self() << std::endl;
    ServerLocalCacheProvider::copper_address_book_name += ".txt";
    std::ofstream out(ServerLocalCacheProvider::copper_address_book_name, std::ios::app);
    if(!out) {
        std::cerr << "Failed to open the file: " << ServerLocalCacheProvider::copper_address_book_name << std::endl;
    } else {
        std::cout << "File opened successfully: " << ServerLocalCacheProvider::copper_address_book_name << std::endl;
    }
    out << serverEngine.self() << std::endl;
    out.close();

    out = std::ofstream(ServerLocalCacheProvider::my_hostname);
    std::cout.rdbuf(out.rdbuf());
    std::cout << "address written" << std::endl;

    std::cout << "parsing address book" << std::endl;
    NodeTree::parse_nodelist_from_cxi_address_book();
    std::cout << "building tree" << std::endl;
    Node::root = NodeTree::build_my_tree(Node::root, ServerLocalCacheProvider::node_address_data);
    NodeTree::printTree(Node::root);
    int tree_depth = NodeTree::depth(Node::root);

    std::cout << "The depth of the tree is :" << tree_depth << std::endl;
    NodeTree::prettyPrintTree(Node::root, tree_depth);

    std::cout << "Server running at address " << serverEngine.self() << std::endl;
    serverEngine.enable_remote_shutdown();
    rpc_lstat = serverEngine.define("rpc_lstat");
    rpc_readfile = serverEngine.define("rpc_readfile");
    new ServerLocalCacheProvider{serverEngine, ServerLocalCacheProvider::node_address_data};
    sleep(5); //  barrier issue: all process need to wait until the server is created.

    std::cout << "Main pid " << getpid() << " " << std::endl;
    pthread_t tid;
    tid = pthread_self();
    std::cout << tid << std::endl;

    if(Node::root->data != static_cast<std::string>(serverEngine.self())) {
        std::cout << "mounting" << std::endl;
        CuFuse::cu_hello_main(argc, argv, &serverEngine);
    } else {
        std::cout << "not mounting" << std::endl;
    }
    std::chrono::time_point<std::chrono::system_clock> start, end;

    std::cout << "Waiting on Finalize " << std::endl;
    serverEngine.wait_for_finalize();
    std::cout << "Successfully killed by remote shutdown process - closing down" << std::endl;

    return 0;

    return 0;
}
