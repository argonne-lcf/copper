#ifndef SERVER_LOCAL_CACHE_PROVIDER_H
#define SERVER_LOCAL_CACHE_PROVIDER_H

#include <memory>
#include <algorithm>
#include <array>
#include <atomic>
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
#include "leased_ptr.h"
#include "../cache/abt_sync.h"
#include "../cache/parent_readiness.h"
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include "byte_vector_serialization.h"  // bulk std::vector<std::byte> (wire-identical)
#include <thread>
#include <unordered_map>
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
    static inline std::vector<std::pair<std::string, std::string>> global_peer_pairs;
    static inline std::vector<std::string> node_address_data;
    static constexpr uint16_t provider_id = 0;

    static inline tl::remote_procedure rpc_lstat;
    static inline tl::remote_procedure rpc_readfile;
    static inline tl::remote_procedure rpc_readdir;
    static inline tl::remote_procedure rpc_is_ready;
    static inline std::atomic<tl::engine*> my_engine{nullptr};
    // Parent/child startup races at scale can lead to HG_NOENTRY if a child
    // forwards an RPC before the parent provider has completed registration.
    // This local flag is exposed through a tiny readiness RPC so children can
    // wait for an actual ready parent instead of relying only on global sleep.
    static inline std::atomic<bool> provider_ready{false};
    static inline std::atomic<long long> provider_ready_after_us{-1};
    static inline std::chrono::steady_clock::time_point provider_start_time;
    // Readiness flag + Argobots mutex + shared-failure stamp for the parent
    // readiness wait (see cache/parent_readiness.h).
    static inline ParentReadiness::Gate parent_provider_ready_gate;
    // Parent endpoint, resolved once in start_thallium_engine() under the
    // startup watchdog, before the provider is published. RPC handlers copy it
    // instead of calling the untimed tl::engine::lookup on every request.
    // Reset in cu_fuse_destroy before engine finalize.
    // Parent endpoint resolved once at startup. Handlers borrow it through a
    // counted lease; cu_fuse_destroy closes it (bounded wait, never frees under
    // a running handler). Review r4 finding 2.
    static inline LeasedPtr<tl::endpoint> parent_endpoint;
    static inline std::atomic<long long> first_successful_parent_rpc_after_us{-1};
    // Short-lived exact-path ENOENT cache used to collapse repeated metadata
    // probes without treating whole parent directories as missing.
    static inline std::unordered_map<std::string, std::chrono::steady_clock::time_point> md_enoent_ttl_cache;
    static inline std::mutex md_enoent_ttl_cache_mtx;

    ServerLocalCacheProvider(const tl::engine& serverEngine, const std::vector<std::string>& addresses)
    : tl::provider<ServerLocalCacheProvider>{serverEngine, provider_id} {
        define("rpc_lstat", &ServerLocalCacheProvider::rpcLstat);
        define("rpc_readfile", &ServerLocalCacheProvider::rpcRead);
        define("rpc_readdir", &ServerLocalCacheProvider::rpcReaddir);
        define("rpc_is_ready", &ServerLocalCacheProvider::rpcIsReady);

        get_engine().push_finalize_callback([this]() { delete this; });
    }

    using lstat_return_type = std::pair<int, std::vector<std::byte>>;
    using lstat_final_return_type = int;
    void rpcLstat(const tl::request& req, bool dest, const std::string& path_string) const;

    using read_return_type = std::pair<ssize_t, std::vector<std::byte>>;
    using read_final_return_type = ssize_t;
    void rpcRead(const tl::request& req, bool dest, const std::string& path_string) const;

    using readdir_return_type = std::pair<int, std::vector<std::string>>;
    using readdir_final_return_type = int;
    void rpcReaddir(const tl::request& req, bool dest, const std::string& path_string) const;

    void rpcIsReady(const tl::request& req) const;

    ServerLocalCacheProvider(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider(ServerLocalCacheProvider&&) = delete;
    ServerLocalCacheProvider& operator=(const ServerLocalCacheProvider&) = delete;
    ServerLocalCacheProvider& operator=(ServerLocalCacheProvider&&) = delete;
};

#endif // SERVER_LOCAL_CACHE_PROVIDER_H
