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
#include <dirent.h>
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

#include "../aixlog.h"
#include "../cache/cache_tables.h"
#include "../fs/util.h"
#include "node_tree.h"

#define RPC_METADATA_TAG "rpc_getattr"
#define RPC_DATA_TAG "rpc_readfile"
#define RPC_READDIR_TAG "rpc_readdir"
#define RPC_TAG "rpc"

#define START_RPC_TIMER(thread_location)                                                                                         \
    LOG(TRACE, RPC_TAG) << __FUNCTION__ << " " << thread_location << " rpc timer for path_string: " << path_string << std::endl; \
    LOG(TRACE, RPC_TAG) << __FUNCTION__ << " " << thread_location << " starting rpc timer" << std::endl;                         \
    auto rpc_start = std::chrono::high_resolution_clock::now();

#define STOP_RPC_TIMER(thread_location)                                                                                              \
    {                                                                                                                                \
        LOG(TRACE, RPC_TAG) << __FUNCTION__ << " " << thread_location << " stopping rpc timer" << std::endl;                         \
        auto rpc_end = std::chrono::high_resolution_clock::now();                                                                    \
        std::chrono::duration<double, std::milli> rpc_diff = rpc_end - rpc_start;                                                    \
        LOG(TRACE, RPC_TAG) << __FUNCTION__ << " " thread_location << " total rpc time: " << rpc_diff.count() << " ms" << std::endl; \
    }

#define TIME_RPC(expression)         \
    START_RPC_TIMER("copper thread") \
    expression;                      \
    STOP_RPC_TIMER("copper thread")

#define TIME_RPC_FUSE_THREAD(expression) \
    START_RPC_TIMER("cu_fuse thread")    \
    expression;                          \
    STOP_RPC_TIMER("cu_fuse thread")

namespace tl = thallium;

class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> {
    public:
    static std::vector<std::pair<std::string, std::string>> global_peer_pairs;
    static std::mutex mtx;
    static std::vector<std::string> node_address_data;
    static std::vector<tl::endpoint> m_peers;
    static constexpr uint16_t provider_id = 0;

    static tl::remote_procedure rpc_lstat;
    static tl::remote_procedure rpc_readfile;
    static tl::remote_procedure rpc_readdir;


    ServerLocalCacheProvider(const tl::engine& serverEngine, const std::vector<std::string>& addresses)
    : tl::provider<ServerLocalCacheProvider>{serverEngine, provider_id} {
        define("rpc_lstat", &ServerLocalCacheProvider::rpcLstat);
        define("rpc_readfile", &ServerLocalCacheProvider::rpcRead);
        define("rpc_readdir", &ServerLocalCacheProvider::rpcReaddir);

        get_engine().push_finalize_callback([this]() { delete this; });
        m_peers.reserve(addresses.size());

        for(auto& address : addresses) {
            m_peers.push_back(get_engine().lookup(address));
        }
    }

    using lstat_return_type = std::pair<int, std::vector<std::byte>>;
    void rpcLstat(const tl::request& req, const std::string& path_string) const;

    using read_return_type = std::pair<int, std::vector<std::byte>>;
    void rpcRead(const tl::request& req, const std::string& path_string) const;

    using readdir_return_type = std::pair<int, std::vector<std::string>>;
    void rpcReaddir(const tl::request& req, const std::string& path_string) const;

    ServerLocalCacheProvider(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider(ServerLocalCacheProvider&&) = delete;
    ServerLocalCacheProvider& operator=(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider& operator=(ServerLocalCacheProvider&&) = delete;
};

#endif // SERVER_LOCAL_CACHE_PROVIDER_H
