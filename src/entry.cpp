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
    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);
    std::string curr_node_addr_server;
    std::string curr_node_addr_client;
    std::string per_node_out_file = hostname + ".log";
    std::ofstream out(per_node_out_file);

    ServerLocalCacheProvider::copper_address_book_name += ".txt";

    std::cout.rdbuf(out.rdbuf());

    NodeTree::get_hsn0_cxi_addr();
    NodeTree::parse_nodelist_from_cxi_address_book();
    Node::root = NodeTree::build_my_tree(Node::root, ServerLocalCacheProvider::node_address_data);
    NodeTree::printTree(Node::root);
    int tree_depth = NodeTree::depth(Node::root);
    std::cout << "The depth of the tree is :" << tree_depth << std::endl;
    NodeTree::prettyPrintTree(Node::root, tree_depth);

    auto serverEngine = tl::engine{"cxi", THALLIUM_SERVER_MODE, 1, -1};
    std::cout << "Server running at address " << serverEngine.self() << std::endl;
    serverEngine.enable_remote_shutdown();
    rpc_lstat = serverEngine.define("rpc_lstat");
    rpc_readfile = serverEngine.define("rpc_readfile");
    new ServerLocalCacheProvider{serverEngine, ServerLocalCacheProvider::node_address_data};
    sleep(20); //  barrier issue: all process need to wait until the server is created.

    std::cout << "Main pid " << getpid() << " " << std::endl;
    pthread_t tid;
    tid = pthread_self();
    std::cout << tid << std::endl;

    CuFuse::cu_hello_main(argc, argv, &serverEngine);
    std::chrono::time_point<std::chrono::system_clock> start, end;

    out.close();

    std::cout << "Waiting on Finalize " << std::endl;
    serverEngine.wait_for_finalize();
    std::cout << "Successfully killed by remote shutdown process - closing down" << std::endl;

    return 0;
}
