#include "server_local_cache_provider.h"

#include <cassert>

#include "../metric/profiling.h"

namespace {
// Start probing quickly, then back off to avoid creating a large probe storm
// when many children are waiting on slow parent registration at scale.
constexpr int parent_ready_initial_retry_sleep_ms = 10;
constexpr int parent_ready_max_retry_sleep_ms = 1000;
// Give large jobs substantially more time to finish provider registration.
constexpr int parent_ready_max_wait_ms = 120000;

int parent_ready_retry_sleep_ms(const int attempts) {
    if(attempts <= 1) {
        return parent_ready_initial_retry_sleep_ms;
    }

    int sleep_ms = parent_ready_initial_retry_sleep_ms;
    for(int i = 1; i < attempts; ++i) {
        sleep_ms *= 2;
        if(sleep_ms >= parent_ready_max_retry_sleep_ms) {
            return parent_ready_max_retry_sleep_ms;
        }
    }
    return sleep_ms;
}

bool metadata_enoent_ttl_hit(const std::string& path_string) {
    std::lock_guard guard(ServerLocalCacheProvider::md_enoent_ttl_cache_mtx);

    const auto ttl_entry = ServerLocalCacheProvider::md_enoent_ttl_cache.find(path_string);
    if(ttl_entry == ServerLocalCacheProvider::md_enoent_ttl_cache.end()) {
        return false;
    }

    const auto age = std::chrono::steady_clock::now() - ttl_entry->second;
    if(age < std::chrono::milliseconds(Constants::md_enoent_ttl_ms)) {
        Profiling::record_md_ttl_serve(path_string);
        LOG(DEBUG, RPC_METADATA_TAG) << "serving metadata ENOENT from "
                                     << Constants::md_enoent_ttl_ms
                                     << "ms TTL cache for path: "
                                     << path_string << std::endl;
        return true;
    }

    Profiling::record_md_ttl_expire(path_string);
    ServerLocalCacheProvider::md_enoent_ttl_cache.erase(ttl_entry);
    return false;
}

void update_metadata_enoent_ttl(const std::string& path_string, const int status) {
    std::lock_guard guard(ServerLocalCacheProvider::md_enoent_ttl_cache_mtx);

    if(status == -ENOENT) {
        ServerLocalCacheProvider::md_enoent_ttl_cache[path_string] = std::chrono::steady_clock::now();
        Profiling::record_md_ttl_store(path_string);
        LOG(DEBUG, RPC_METADATA_TAG) << "stored metadata ENOENT in "
                                     << Constants::md_enoent_ttl_ms
                                     << "ms TTL cache for path: "
                                     << path_string << std::endl;
    } else {
        if(ServerLocalCacheProvider::md_enoent_ttl_cache.erase(path_string) > 0) {
            Profiling::record_md_ttl_clear(path_string);
        }
    }
}

bool wait_for_parent_provider_ready(const tl::engine& engine,
                                    const std::string& parent_addr,
                                    const std::string& path_string) {
    const auto wait_start = std::chrono::steady_clock::now();
    auto parent_endpoint = engine.lookup(parent_addr);
    tl::provider_handle parent_ph(parent_endpoint, ServerLocalCacheProvider::provider_id);

    int attempts = 0;
    while(true) {
        ++attempts;
        try {
            const bool parent_ready = ServerLocalCacheProvider::rpc_is_ready.on(parent_ph)();
            if(parent_ready) {
                const auto wait_end = std::chrono::steady_clock::now();
                LOG(INFO, RPC_METADATA_TAG) << "parent readiness confirmed for path " << path_string
                                            << " at parent " << parent_addr
                                            << " after "
                                            << std::chrono::duration_cast<std::chrono::microseconds>(wait_end - wait_start).count()
                                            << " us across " << attempts << " probe(s)" << std::endl;
                return true;
            }
        } catch(const tl::exception& ex) {
            LOG(WARNING, RPC_METADATA_TAG) << "thallium exception while probing parent readiness for path "
                                           << path_string << " via parent " << parent_addr
                                           << ": " << ex.what() << std::endl;
        } catch(const std::exception& ex) {
            LOG(WARNING, RPC_METADATA_TAG) << "std::exception while probing parent readiness for path "
                                           << path_string << " via parent " << parent_addr
                                           << ": " << ex.what() << std::endl;
        }

        const auto waited_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wait_start).count();
        if(waited_ms >= parent_ready_max_wait_ms) {
            LOG(ERROR, RPC_METADATA_TAG) << "timed out waiting for parent readiness for path " << path_string
                                         << " via parent " << parent_addr
                                         << " after " << waited_ms << " ms and " << attempts
                                         << " probe(s); max_wait_ms=" << parent_ready_max_wait_ms << std::endl;
            return false;
        }

        const int retry_sleep_ms = parent_ready_retry_sleep_ms(attempts);
        if(attempts == 1 || attempts % 25 == 0) {
            LOG(INFO, RPC_METADATA_TAG) << "parent not ready yet for path " << path_string
                                        << " via parent " << parent_addr
                                        << ", waited " << waited_ms
                                        << " ms so far; attempts=" << attempts
                                        << "; next_retry_sleep_ms=" << retry_sleep_ms << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
    }
}

bool ensure_parent_provider_ready(const tl::engine& engine,
                                  const std::string& parent_addr,
                                  const std::string& path_string) {
    if(ServerLocalCacheProvider::parent_provider_ready_cached.load(std::memory_order_acquire)) {
        return true;
    }

    std::lock_guard<std::mutex> guard(ServerLocalCacheProvider::parent_provider_ready_mtx);
    if(ServerLocalCacheProvider::parent_provider_ready_cached.load(std::memory_order_relaxed)) {
        return true;
    }

    const bool parent_ready = wait_for_parent_provider_ready(engine, parent_addr, path_string);
    if(parent_ready) {
        ServerLocalCacheProvider::parent_provider_ready_cached.store(true, std::memory_order_release);
    }
    return parent_ready;
}

void note_first_successful_parent_rpc(const char* rpc_name, const std::string& path_string) {
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - ServerLocalCacheProvider::provider_start_time)
            .count();
    long long expected = -1;
    if(ServerLocalCacheProvider::first_successful_parent_rpc_after_us.compare_exchange_strong(expected, elapsed_us)) {
        LOG(INFO) << "first successful parent " << rpc_name
                  << " completed after " << elapsed_us
                  << " us since provider startup; path=" << path_string << std::endl;
    }
}
} // namespace

void ServerLocalCacheProvider::rpcIsReady(const tl::request& req) const {
    req.respond(provider_ready.load());
}

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(DEBUG, RPC_METADATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(DEBUG, RPC_METADATA_TAG) << "req_coming_from_addr:" << req_from_addr << ", path_string: " << path_string
                                 << std::endl;

    lstat_return_type lstat_response;
    bool cached = false;
    bool served_from_negative_ttl = false;
    const auto entry_opt = CacheTables::md_cache_table.get(path_string);

    if(entry_opt.has_value()) {
        LOG(DEBUG, RPC_METADATA_TAG) << "found in local cache" << std::endl;
        lstat_response = lstat_return_type(Constants::fs_operation_success, entry_opt.value()->get_vec());
        cached = true;
        update_metadata_enoent_ttl(path_string, Constants::fs_operation_success);
    } else if(metadata_enoent_ttl_hit(path_string)) {
        lstat_response = lstat_return_type(-ENOENT, std::vector<std::byte>(0));
        cached = true;
        served_from_negative_ttl = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_METADATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        CuStat cu_stat;
        if(lstat(path_string.c_str(), cu_stat.get_st()) == -1) {
            const int saved_errno = errno;
            if(saved_errno == ENOENT) {
                // Python and Conda startup probe many optional paths. Missing-path
                // metadata lookups are expected and should not look like failures.
                LOG(DEBUG, RPC_METADATA_TAG) << "storage stat for path: " << path_string
                                             << " returned with errno: " << saved_errno << std::endl;
            } else {
                LOG(ERROR) << "storage stat for path: " << path_string
                           << " returned with errno: " << saved_errno << std::endl;
            }
            lstat_response = lstat_return_type(-saved_errno, std::vector<std::byte>(0));
        } else {
            lstat_response = lstat_return_type(Constants::fs_operation_success, cu_stat.get_vec());
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        if(!CacheTables::md_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(DEBUG, RPC_METADATA_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                auto parent_endpoint = get_engine().lookup(Node::parent_addr.value());
                if(!ensure_parent_provider_ready(get_engine(), Node::parent_addr.value(), path_string)) {
                    lstat_response = lstat_return_type(-EIO, std::vector<std::byte>(0));
                } else {
                    tl::provider_handle parent_ph(parent_endpoint, provider_id);
                    lstat_response = rpc_lstat.on(parent_ph)(false, path_string);
                    if(lstat_response.first == Constants::fs_operation_success) {
                        note_first_successful_parent_rpc("rpc_lstat", path_string);
                    }
                }
            } catch(const tl::exception& ex) {
                LOG(ERROR, RPC_METADATA_TAG) << "thallium exception during parent rpc for path " << path_string
                                             << " via parent " << Node::parent_addr.value()
                                             << ": " << ex.what() << std::endl;
                lstat_response = lstat_return_type(-EIO, std::vector<std::byte>(0));
            } catch(const std::exception& ex) {
                LOG(ERROR, RPC_METADATA_TAG) << "std::exception during parent rpc for path " << path_string
                                             << " via parent " << Node::parent_addr.value()
                                             << ": " << ex.what() << std::endl;
                lstat_response = lstat_return_type(-EIO, std::vector<std::byte>(0));
            }
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            LOG(DEBUG, RPC_METADATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            int status = CacheTables::md_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != 0) {
                lstat_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                assert(CacheTables::md_cache_table.get(path_string).has_value());
                lstat_response = std::make_pair(status, CacheTables::md_cache_table.get(path_string).value()->get_vec());
            }

            cached = true;
        }
#endif
    }

    if(!served_from_negative_ttl) {
        update_metadata_enoent_ttl(path_string, lstat_response.first);
    }

    if(!cached && lstat_response.first == Constants::fs_operation_success) {
        LOG(DEBUG, RPC_METADATA_TAG) << "caching intermediate rpc_lstat_response" << std::endl;
        CacheTables::md_cache_table.put_force(path_string, CuStat{lstat_response.second});
    } else if(cached) {
        LOG(DEBUG, RPC_METADATA_TAG) << "lstat response already cached" << std::endl;
    } else {
        LOG(DEBUG, RPC_METADATA_TAG) << "lstat response != 0, not caching intermediate rpc_lstat_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::md_path_status_cache_table.update_cache_status(path_string, lstat_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(lstat_response));
    } else {
        LOG(DEBUG, RPC_METADATA_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(lstat_response.first));
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(DEBUG, RPC_DATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(DEBUG, RPC_DATA_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string << std::endl;

    read_return_type read_response;
    bool cached = false;
    const auto entry_opt = CacheTables::data_cache_table.get(path_string);

    if(!dest && entry_opt.has_value()) {
        LOG(DEBUG, RPC_DATA_TAG) << "found in local cache" << std::endl;
        const auto bytes = entry_opt.value();
        read_response = std::make_pair(bytes->size(), *bytes);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_DATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        try {
            const std::vector<std::byte>& file_bytes = Util::read_ent_file(path_string);
            read_response = std::make_pair(file_bytes.size(), file_bytes);
        } catch(std::exception& e) {
            LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
            read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        if(!CacheTables::data_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(DEBUG, RPC_DATA_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                auto parent_endpoint = get_engine().lookup(Node::parent_addr.value());
                if(!ensure_parent_provider_ready(get_engine(), Node::parent_addr.value(), path_string)) {
                    read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
                } else {
                    tl::provider_handle parent_ph(parent_endpoint, provider_id);
                    read_response = rpc_readfile.on(parent_ph)(false, path_string);
                    if(read_response.first >= 0) {
                        note_first_successful_parent_rpc("rpc_readfile", path_string);
                    }
                }
            } catch(const tl::exception& ex) {
                LOG(ERROR, RPC_DATA_TAG) << "thallium exception during parent rpc for path " << path_string
                                         << " via parent " << Node::parent_addr.value()
                                         << ": " << ex.what() << std::endl;
                read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
            } catch(const std::exception& ex) {
                LOG(ERROR, RPC_DATA_TAG) << "std::exception during parent rpc for path " << path_string
                                         << " via parent " << Node::parent_addr.value()
                                         << ": " << ex.what() << std::endl;
                read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
            }
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            LOG(DEBUG, RPC_DATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            int status = CacheTables::data_path_status_cache_table.wait_on_cache_status(path_string);

            if(status < 0) {
                read_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                assert(CacheTables::data_cache_table.get(path_string).has_value());
                read_response = std::make_pair(status, *CacheTables::data_cache_table.get(path_string).value());
            }

            cached = true;
        }
#endif
    }

    if(!cached && read_response.first >= 0) {
        LOG(DEBUG, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        CacheTables::data_cache_table.put_force(path_string, std::vector<std::byte>(read_response.second));
    } else if(cached) {
        LOG(DEBUG, RPC_DATA_TAG) << "readfile response already cached" << std::endl;
    } else {
        LOG(DEBUG, RPC_DATA_TAG) << "readfile response < 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::data_path_status_cache_table.update_cache_status(path_string, read_response.first);
#endif

    if(!dest) {
        LOG(DEBUG, RPC_DATA_TAG) << "byte size: " << read_response.second.size() << std::endl;
        TIME_RPC(req.respond(read_response));
    } else {
        LOG(DEBUG, RPC_DATA_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(read_response.first));
    }
}

void ServerLocalCacheProvider::rpcReaddir(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(DEBUG, RPC_READDIR_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(DEBUG, RPC_READDIR_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string
                                << std::endl;

    readdir_return_type readdir_response;
    bool cached = false;
    const auto entry_opt = CacheTables::tree_cache_table.get(path_string);

    if(!dest && entry_opt.has_value()) {
        LOG(DEBUG, RPC_READDIR_TAG) << "found in local cache" << std::endl;
        const auto paths = entry_opt.value();
        readdir_response = std::make_pair(Constants::fs_operation_success, *paths);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_READDIR_TAG) << "requesting data from underlying filesystem" << std::endl;
        auto entries = std::vector<std::string>{};
        DIR* dp = opendir(path_string.c_str());
        if(dp == nullptr) {
            const int saved_errno = errno;
            if(saved_errno == ENOENT || saved_errno == ENOTDIR) {
                LOG(DEBUG, RPC_READDIR_TAG) << "storage opendir for path: " << path_string
                                            << " returned with errno: " << saved_errno << std::endl;
            } else {
                LOG(WARNING) << "failed to passthrough readdir" << std::endl;
                LOG(ERROR) << "storage opendir for path: " << path_string
                           << " returned with errno: " << saved_errno << std::endl;
            }
            readdir_response = std::make_pair(-saved_errno, std::vector<std::string>(0));
        } else {
            dirent* de;
            errno = 0;
            while((de = readdir(dp)) != nullptr) {
                entries.emplace_back(de->d_name);
                errno = 0;
            }

            if(errno != 0) {
                const int saved_errno = errno;
                LOG(ERROR) << "storage readdir for path: " << path_string
                           << " returned with errno: " << saved_errno << std::endl;
                readdir_response = std::make_pair(-saved_errno, std::vector<std::string>(0));
            } else {
                readdir_response = std::make_pair(Constants::fs_operation_success, entries);
            }

            closedir(dp);
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        if(!CacheTables::tree_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(DEBUG, RPC_READDIR_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                auto parent_endpoint = get_engine().lookup(Node::parent_addr.value());
                if(!ensure_parent_provider_ready(get_engine(), Node::parent_addr.value(), path_string)) {
                    readdir_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::string>(0));
                } else {
                    tl::provider_handle parent_ph(parent_endpoint, provider_id);
                    readdir_response = rpc_readdir.on(parent_ph)(false, path_string);
                    if(readdir_response.first == Constants::fs_operation_success) {
                        note_first_successful_parent_rpc("rpc_readdir", path_string);
                    }
                }
            } catch(const tl::exception& ex) {
                LOG(ERROR, RPC_READDIR_TAG) << "thallium exception during parent rpc for path " << path_string
                                            << " via parent " << Node::parent_addr.value()
                                            << ": " << ex.what() << std::endl;
                readdir_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::string>(0));
            } catch(const std::exception& ex) {
                LOG(ERROR, RPC_READDIR_TAG) << "std::exception during parent rpc for path " << path_string
                                            << " via parent " << Node::parent_addr.value()
                                            << ": " << ex.what() << std::endl;
                readdir_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::string>(0));
            }
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            LOG(DEBUG, RPC_READDIR_TAG) << "not first to request path_string, waiting on cache entry..." << std::endl;
            const auto status = CacheTables::tree_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != Constants::fs_operation_success) {
                readdir_response = std::make_pair(status, std::vector<std::string>(0));
            } else {
                assert(CacheTables::tree_cache_table.get(path_string).has_value());
                readdir_response = std::make_pair(status, *CacheTables::tree_cache_table.get(path_string).value());
            }

            cached = true;
        }
#endif
    }

    if(!cached && readdir_response.first == Constants::fs_operation_success) {
        LOG(DEBUG, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        CacheTables::tree_cache_table.put_force(path_string, std::vector<std::string>(readdir_response.second));
    } else if(cached) {
        LOG(DEBUG, RPC_READDIR_TAG) << "readdir response already cached" << std::endl;
    } else {
        LOG(DEBUG, RPC_READDIR_TAG) << "readdir response != 0, not caching intermediate rpc_readfile_response"
                                    << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::tree_path_status_cache_table.update_cache_status(path_string, readdir_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(readdir_response));
    } else {
        LOG(DEBUG, RPC_READDIR_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(readdir_response.first));
    }
}
