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
#include "fs/util.h"

int main(int argc, char** argv) {
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);
    LOG(TRACE) << " " << std::endl;

    auto new_args{Util::process_args(argc, (const char**)argv)};

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);

    if(Constants::log_type == "stdout") {
        AixLog::Log::init({std::make_shared<AixLog::SinkCout>(static_cast<AixLog::Severity>(Constants::log_level))});
    } else if(Constants::log_type == "file") {
        auto output_file = Constants::log_output_dir.value() + "/" + hostname + ".log";
        LOG(INFO) << "output_file path: " << output_file << std::endl;

        AixLog::Log::init({std::make_shared<AixLog::SinkFile>(static_cast<AixLog::Severity>(Constants::log_level), output_file)});
    } else if(Constants::log_type == "file_and_stdout") {
        auto output_file = Constants::log_output_dir.value() + "/" + hostname + ".log";
        LOG(INFO) << "output_file path: " << output_file << std::endl;

        AixLog::Log::init({std::make_shared<AixLog::SinkCout>(static_cast<AixLog::Severity>(Constants::log_level)),
        std::make_shared<AixLog::SinkFile>(static_cast<AixLog::Severity>(Constants::log_level), output_file)});
    }


    Constants::copper_address_book_path = Constants::log_output_dir.value() + "/" + Constants::copper_address_book_filename;
    std::ofstream copper_address_book(Constants::copper_address_book_path);
    // Check if the file was successfully opened
    if (!copper_address_book.is_open()) {
        LOG(FATAL) << "unable to create and open copper address book at path: " << Constants::copper_address_book_path << std::endl;
        throw std::runtime_error("unable to create and open copper address book");
    }

    std::vector<char*> ptrs;
    ptrs.reserve(new_args.size());
    for(std::string& str : new_args) {
        ptrs.push_back(str.data());
    }

    NodeTree::get_hsn0_cxi_addr();
    NodeTree::parse_nodelist_from_cxi_address_book();
    Node::root = NodeTree::build_my_tree(Node::root, ServerLocalCacheProvider::node_address_data);
    NodeTree::printTree(Node::root);
    int tree_depth = NodeTree::depth(Node::root);
    LOG(INFO) << "the depth of the tree is: " << tree_depth << std::endl;
    NodeTree::prettyPrintTree(Node::root, tree_depth);

    auto serverEngine = tl::engine{"cxi", THALLIUM_SERVER_MODE, 1, -1};
    LOG(INFO) << "server running at address: " << serverEngine.self() << std::endl;
    serverEngine.enable_remote_shutdown();
    rpc_lstat = serverEngine.define("rpc_lstat");
    rpc_readfile = serverEngine.define("rpc_readfile");
    rpc_readdir = serverEngine.define("rpc_readdir");
    new ServerLocalCacheProvider{serverEngine, ServerLocalCacheProvider::node_address_data};
    sleep(20); //  barrier issue: all process need to wait until the server is created.

    LOG(INFO) << "main pid: " << getpid() << " " << std::endl;
    pthread_t tid;
    tid = pthread_self();
    LOG(INFO) << tid << std::endl;

    CuFuse::cu_hello_main(ptrs.size(), ptrs.data(), &serverEngine);
    std::chrono::time_point<std::chrono::system_clock> start, end;

    LOG(INFO) << "waiting on finalize..." << std::endl;
    serverEngine.wait_for_finalize();
    LOG(INFO) << "successfully killed by remote shutdown process - closing down" << std::endl;

    return 0;
}
