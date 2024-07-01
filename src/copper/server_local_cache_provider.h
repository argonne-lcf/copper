#ifndef SERVER_LOCAL_CACHE_PROVIDER_H
#define SERVER_LOCAL_CACHE_PROVIDER_H

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

#include "../fs/util.h"
#include "node_tree.h"

namespace tl = thallium;

extern tl::remote_procedure rpc_lstat;
extern tl::remote_procedure rpc_readfile;

class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> {
    public:
    static std::vector<std::pair<std::string, std::string>> global_peer_pairs;
    static std::mutex mtx;
    static std::vector<std::string> node_address_data;
    static std::string copper_address_book_name;
    static std::vector<tl::endpoint> m_peers;
    static const uint16_t provider_id = 0;
    static std::string my_hostname;


    ServerLocalCacheProvider(tl::engine& serverEngine, const std::vector<std::string>& addresses)
    : tl::provider<ServerLocalCacheProvider>{serverEngine, provider_id} {
        define("rpc_lstat", &ServerLocalCacheProvider::rpcLstat);
        define("rpc_readfile", &ServerLocalCacheProvider::rpcRead);

        get_engine().push_finalize_callback([this]() { delete this; });
        m_peers.reserve(addresses.size());

        for(auto& address : addresses) {
            m_peers.push_back(get_engine().lookup(address));
        }
    }

    using lstat_return_type = std::pair<int, std::vector<std::byte>>;
    void rpcLstat(const tl::request& req, const std::string& path_string);

    using read_return_type = std::pair<int, std::vector<std::byte>>;
    void rpcRead(const tl::request& req, const std::string& path_string);

    static void getParentfromtree(Node* CopyofTree, std::string my_curr_node_addr, std::string& parentofmynode);

    ServerLocalCacheProvider(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider(ServerLocalCacheProvider&&) = delete;
    ServerLocalCacheProvider& operator=(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider& operator=(ServerLocalCacheProvider&&) = delete;
};

#endif //SERVER_LOCAL_CACHE_PROVIDER_H
