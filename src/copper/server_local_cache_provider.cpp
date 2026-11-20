#include "server_local_cache_provider.h"
#include "../fs/error_status.h"
#include "../fs/blocking_offload.h"
#include "root_storage.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>

#include "../metric/profiling.h"

namespace {
// The TTL map lock is an OS mutex taken from RPC handler ULTs: keep the
// critical section to map operations only; logging and profiling happen after
// the lock is released.
bool metadata_enoent_ttl_hit(const std::string& path_string) {
    enum class Hit { miss, serve, expired } hit = Hit::miss;
    {
        std::lock_guard guard(ServerLocalCacheProvider::md_enoent_ttl_cache_mtx);
        const auto ttl_entry = ServerLocalCacheProvider::md_enoent_ttl_cache.find(path_string);
        if(ttl_entry != ServerLocalCacheProvider::md_enoent_ttl_cache.end()) {
            const auto age = std::chrono::steady_clock::now() - ttl_entry->second;
            if(age < std::chrono::milliseconds(Constants::md_enoent_ttl_ms)) {
                hit = Hit::serve;
            } else {
                ServerLocalCacheProvider::md_enoent_ttl_cache.erase(ttl_entry);
                hit = Hit::expired;
            }
        }
    }
    if(hit == Hit::serve) {
        Profiling::record_md_ttl_serve(path_string);
        LOG(DEBUG, RPC_METADATA_TAG) << "serving metadata ENOENT from "
                                     << Constants::md_enoent_ttl_ms
                                     << "ms TTL cache for path: "
                                     << path_string << std::endl;
        return true;
    }
    if(hit == Hit::expired) Profiling::record_md_ttl_expire(path_string);
    return false;
}

void update_metadata_enoent_ttl(const std::string& path_string, const int status) {
    bool stored = false, cleared = false;
    {
        std::lock_guard guard(ServerLocalCacheProvider::md_enoent_ttl_cache_mtx);
        if(status == -ENOENT) {
            ServerLocalCacheProvider::md_enoent_ttl_cache[path_string] = std::chrono::steady_clock::now();
            stored = true;
        } else {
            cleared = ServerLocalCacheProvider::md_enoent_ttl_cache.erase(path_string) > 0;
        }
    }
    if(stored) {
        Profiling::record_md_ttl_store(path_string);
        LOG(DEBUG, RPC_METADATA_TAG) << "stored metadata ENOENT in "
                                     << Constants::md_enoent_ttl_ms
                                     << "ms TTL cache for path: "
                                     << path_string << std::endl;
    } else if(cleared) {
        Profiling::record_md_ttl_clear(path_string);
    }
}

long long parent_ready_deadline_ms() {
    // Same clock as ParentReadiness::ensure's default now_ms(); taken at the
    // start of the handler's parent path.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
               .count() +
           Constants::parent_ready_timeout_ms;
}

bool ensure_parent_provider_ready(const tl::engine& engine,
                                  const tl::endpoint& parent_endpoint,
                                  const std::string& parent_addr,
                                  const std::string& path_string,
                                  const long long deadline_ms) {
    const tl::provider_handle parent_ph(parent_endpoint, ServerLocalCacheProvider::provider_id);
    ParentReadiness::Hooks hooks;
    // Each probe is a timed RPC, so a parent that never answers cannot hold
    // parent_provider_ready_mtx past the deadline (timeouts surface as
    // tl::timeout, a std::exception).
    hooks.probe = [&](const long long timeout_ms) {
        try {
            const bool ready = ServerLocalCacheProvider::rpc_is_ready.on(parent_ph)
                                   .timed(std::chrono::milliseconds(timeout_ms));
            return ready ? ParentReadiness::Probe::ready : ParentReadiness::Probe::not_ready;
        } catch(const std::exception& ex) {
            LOG(WARNING, RPC_METADATA_TAG) << "parent readiness probe failed for path " << path_string
                                           << " via parent " << parent_addr << " (probe_timeout_ms="
                                           << timeout_ms << "): " << ex.what() << std::endl;
            return ParentReadiness::Probe::error;
        }
    };
    // Yields the ULT; std::this_thread::sleep_for would park the handler
    // execution stream and stall every other RPC on it.
    hooks.sleep_ms = [&](const long long ms) { tl::thread::sleep(engine, static_cast<double>(ms)); };
    hooks.on_retry = [&](const int attempts, const long long waited_ms, const long long next_sleep_ms) {
        if(attempts == 1 || attempts % 25 == 0) {
            LOG(INFO, RPC_METADATA_TAG) << "parent not ready yet for path " << path_string << " via parent "
                                        << parent_addr << ", waited " << waited_ms << " ms so far; attempts="
                                        << attempts << "; next_retry_sleep_ms=" << next_sleep_ms << std::endl;
        }
    };
    hooks.on_ready = [&](const int attempts, const long long waited_ms) {
        LOG(INFO, RPC_METADATA_TAG) << "parent readiness confirmed for path " << path_string << " at parent "
                                    << parent_addr << " after " << waited_ms * 1000 << " us across " << attempts
                                    << " probe(s)" << std::endl;
    };
    hooks.on_timeout = [&](const int attempts, const long long waited_ms) {
        LOG(ERROR, RPC_METADATA_TAG) << "timed out waiting for parent readiness for path " << path_string
                                     << " via parent " << parent_addr << " after " << waited_ms << " ms and "
                                     << attempts << " probe(s); max_wait_ms=" << Constants::parent_ready_timeout_ms
                                     << std::endl;
    };
    hooks.on_shared_failure = [&]() {
        LOG(ERROR, RPC_METADATA_TAG) << "parent readiness wait that covered this request already timed out; "
                                     << "failing path " << path_string << " via parent " << parent_addr
                                     << " without a new wait" << std::endl;
    };

    ParentReadiness::Config config;
    config.timeout_ms = Constants::parent_ready_timeout_ms;
    config.deadline_ms = deadline_ms;
    return ParentReadiness::ensure(ServerLocalCacheProvider::parent_provider_ready_gate, config, hooks);
}

// Storage calls from handler ULTs run on offload worker threads so a slow or
// hung filesystem call cannot park the handler execution stream (see
// fs/blocking_offload.h). Intentionally leaked: detached workers may still be
// inside a hung call at exit.
CopperOffload::BlockingOffload& storage_offload() {
    static auto* pool = new CopperOffload::BlockingOffload(
        static_cast<std::size_t>(Constants::storage_offload_threads),
        static_cast<std::size_t>(Constants::storage_offload_queue));
    return *pool;
}

// Yields the handler ULT between completion polls: first plain yields (fast
// calls finish within a few), then short Argobots-aware sleeps capped at 2 ms.
void storage_wait_step(const tl::engine& engine, const int attempt) {
    if(attempt < 16) {
        tl::thread::yield();
    } else {
        const double ms = std::min(2.0, 0.05 * static_cast<double>(1 << std::min(attempt - 16, 6)));
        tl::thread::sleep(engine, ms);
    }
}

// Runs the closure built by `make_op` on the offload pool, bounded by
// -storage_call_timeout_ms, and always returns a reply value: timed out ->
// -ETIMEDOUT, queue full / no worker -> -EAGAIN, exception or submission
// failure -> -EIO. Pool acquisition (first use constructs it), closure
// construction (path copy, shared flags) and std::function conversion all run
// inside RootStorage::guarded_reply's try block (review r8 finding 1), so a
// failure there also becomes -EIO. Arguments here capture only references,
// so evaluating them does not allocate. Non-done outcomes are logged at ERROR.
template <typename Reply, typename Payload, typename MakeOp>
Reply root_storage_reply(const tl::engine& engine, const char* what, const std::string& path_string,
                         MakeOp&& make_op) noexcept {
    return RootStorage::guarded_reply<Reply>(
        []() -> CopperOffload::BlockingOffload& { return storage_offload(); }, make_op,
        CopperOffload::steady_now_ms() + Constants::storage_call_timeout_ms,
        [&engine](const int attempt) { storage_wait_step(engine, attempt); },
        [](const int status) noexcept { return Reply(status, Payload{}); },
        [what, &path_string](CopperOffload::Outcome outcome, int status, const std::string& message) {
            const char* kind = outcome == CopperOffload::Outcome::timed_out  ? "did not finish within the deadline"
                               : outcome == CopperOffload::Outcome::rejected ? "rejected (offload queue full or no worker)"
                                                                             : "failed";
            LOG(ERROR) << "storage " << what << " for path: " << path_string << " " << kind << " (deadline "
                       << Constants::storage_call_timeout_ms << " ms); replying " << status
                       << (message.empty() ? "" : ": ") << message << std::endl;
        },
        [what, &path_string](const char* error) {
            LOG(ERROR) << "storage " << what << " for path: " << path_string
                       << " could not be started: " << error << "; replying " << -EIO << std::endl;
        });
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
#ifdef BLOCK_REDUNDANT_RPCS
    std::optional<PathStatusCache::Request> path_request;
#endif
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
        lstat_response = lstat_return_type(Constants::fs_operation_success, entry_opt->get_vec());
        cached = true;
        update_metadata_enoent_ttl(path_string, Constants::fs_operation_success);
    } else if(metadata_enoent_ttl_hit(path_string)) {
        lstat_response = lstat_return_type(-ENOENT, std::vector<std::byte>(0));
        cached = true;
        served_from_negative_ttl = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_METADATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        lstat_response = root_storage_reply<lstat_return_type, std::vector<std::byte>>(
            get_engine(), "stat", path_string, [&path_string] {
                return [path = path_string]() -> lstat_return_type {
                    CuStat cu_stat;
                    if(lstat(path.c_str(), cu_stat.get_st()) == -1) {
                        return lstat_return_type(-errno, std::vector<std::byte>(0));
                    }
                    return lstat_return_type(Constants::fs_operation_success, cu_stat.get_vec());
                };
            });
        const int saved_errno = -lstat_response.first;
        if(saved_errno == ENOENT) {
            // Python and Conda startup probe many optional paths. Missing-path
            // metadata lookups are expected and should not look like failures.
            LOG(DEBUG, RPC_METADATA_TAG) << "storage stat for path: " << path_string
                                         << " returned with errno: " << saved_errno << std::endl;
        } else if(saved_errno > 0 && saved_errno != ETIMEDOUT && saved_errno != EAGAIN && saved_errno != EIO) {
            LOG(ERROR) << "storage stat for path: " << path_string << " returned with errno: " << saved_errno
                       << std::endl;
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        path_request = CacheTables::md_path_status_cache_table.begin(path_string);
        if(path_request->is_leader()) {
            LOG(DEBUG, RPC_METADATA_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                const long long ready_deadline_ms = parent_ready_deadline_ms();
                // Lease first: keeps parent_endpoint alive across shutdown and is
                // destroyed after every endpoint/provider_handle copy below.
                const LeasedPtr<tl::endpoint>::Lease parent_lease(ServerLocalCacheProvider::parent_endpoint);
                const tl::endpoint& parent_endpoint = parent_lease.get();
                if(!ensure_parent_provider_ready(get_engine(), parent_endpoint, Node::parent_addr.value(), path_string,
                                                 ready_deadline_ms)) {
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
            const int status = static_cast<int>(CacheTables::md_path_status_cache_table.wait(*path_request));

            if(status != 0) {
                lstat_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                const auto entry = CacheTables::md_cache_table.get(path_string);
                lstat_response = entry ? std::make_pair(status, entry->get_vec())
                                       : std::make_pair(-EIO, std::vector<std::byte>{});
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
    if(path_request && path_request->is_leader())
        CacheTables::md_path_status_cache_table.complete(*path_request, lstat_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(lstat_response));
    } else {
        LOG(DEBUG, RPC_METADATA_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(lstat_response.first));
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const bool dest, const std::string& path_string) const {
#ifdef BLOCK_REDUNDANT_RPCS
    std::optional<PathStatusCache::Request> path_request;
#endif
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
        read_response = std::make_pair(bytes.size(), bytes);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_DATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        // read_ent_file throws on open/fstat/read errors; the worker turns that
        // into -EIO (as before this change), so the offload only adds
        // -ETIMEDOUT / -EAGAIN, and -EIO for a failed submission.
        read_response = root_storage_reply<read_return_type, std::vector<std::byte>>(
            get_engine(), "read", path_string, [&path_string] {
                return [path = path_string]() -> read_return_type {
                    try {
                        auto bytes = Util::read_ent_file(path);
                        const auto size = static_cast<ssize_t>(bytes.size());
                        return read_return_type(size, std::move(bytes));
                    } catch(const std::exception& e) {
                        LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
                        return std::make_pair(-EIO, std::vector<std::byte>(0));
                    }
                };
            });
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        path_request = CacheTables::data_path_status_cache_table.begin(path_string);
        if(path_request->is_leader()) {
            LOG(DEBUG, RPC_DATA_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                const long long ready_deadline_ms = parent_ready_deadline_ms();
                // Lease first: keeps parent_endpoint alive across shutdown and is
                // destroyed after every endpoint/provider_handle copy below.
                const LeasedPtr<tl::endpoint>::Lease parent_lease(ServerLocalCacheProvider::parent_endpoint);
                const tl::endpoint& parent_endpoint = parent_lease.get();
                if(!ensure_parent_provider_ready(get_engine(), parent_endpoint, Node::parent_addr.value(), path_string,
                                                 ready_deadline_ms)) {
                    read_response = std::make_pair(-EIO, std::vector<std::byte>(0));
                } else {
                    tl::provider_handle parent_ph(parent_endpoint, provider_id);
                    read_response = rpc_readfile.on(parent_ph)(false, path_string);
                    if(CopperError::is_success(read_response.first)) {
                        note_first_successful_parent_rpc("rpc_readfile", path_string);
                    }
                }
            } catch(const tl::exception& ex) {
                LOG(ERROR, RPC_DATA_TAG) << "thallium exception during parent rpc for path " << path_string
                                         << " via parent " << Node::parent_addr.value()
                                         << ": " << ex.what() << std::endl;
                read_response = std::make_pair(-EIO, std::vector<std::byte>(0));
            } catch(const std::exception& ex) {
                LOG(ERROR, RPC_DATA_TAG) << "std::exception during parent rpc for path " << path_string
                                         << " via parent " << Node::parent_addr.value()
                                         << ": " << ex.what() << std::endl;
                read_response = std::make_pair(-EIO, std::vector<std::byte>(0));
            }
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            LOG(DEBUG, RPC_DATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            const ssize_t status = CacheTables::data_path_status_cache_table.wait(*path_request);

            if(status < 0) {
                read_response = read_return_type(status, std::vector<std::byte>(0));
            } else {
                const auto entry = CacheTables::data_cache_table.get(path_string);
                read_response = entry ? read_return_type(status, *entry)
                                      : read_return_type(-EIO, std::vector<std::byte>{});
            }

            cached = true;
        }
#endif
    }

    if(!cached && CopperError::is_success(read_response.first)) {
        LOG(DEBUG, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        CacheTables::data_cache_table.put_force(path_string, std::vector<std::byte>(read_response.second));
    } else if(cached) {
        LOG(DEBUG, RPC_DATA_TAG) << "readfile response already cached" << std::endl;
    } else {
        LOG(DEBUG, RPC_DATA_TAG) << "readfile response < 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    if(path_request && path_request->is_leader())
        CacheTables::data_path_status_cache_table.complete(*path_request, read_response.first);
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
#ifdef BLOCK_REDUNDANT_RPCS
    std::optional<PathStatusCache::Request> path_request;
#endif
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
        readdir_response = std::make_pair(Constants::fs_operation_success, paths);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(DEBUG, RPC_READDIR_TAG) << "requesting data from underlying filesystem" << std::endl;
        // opendir/readdir/closedir run together on one worker (RAII DIR handle).
        // The opendir-vs-readdir flag only selects the log level; it is atomic
        // because a timed-out worker may still set it after we read it. It is
        // allocated inside the guarded preparation (make_op); assigning a
        // shared_ptr is noexcept, and a null flag (preparation failed) reads
        // as false.
        std::shared_ptr<std::atomic<bool>> opendir_failed;
        readdir_response = root_storage_reply<readdir_return_type, std::vector<std::string>>(
            get_engine(), "readdir", path_string, [&path_string, &opendir_failed] {
                auto flag = std::make_shared<std::atomic<bool>>(false);
                opendir_failed = flag;
                return [path = path_string, flag]() -> readdir_return_type {
                    auto listing = RootStorage::list_directory(path);
                    flag->store(listing.opendir_failed, std::memory_order_relaxed);
                    if(listing.status != 0) return readdir_return_type(listing.status, std::vector<std::string>(0));
                    return readdir_return_type(Constants::fs_operation_success, std::move(listing.entries));
                };
            });
        const int saved_errno = -readdir_response.first;
        const bool from_opendir = opendir_failed && opendir_failed->load(std::memory_order_relaxed);
        if(from_opendir && (saved_errno == ENOENT || saved_errno == ENOTDIR)) {
            LOG(DEBUG, RPC_READDIR_TAG) << "storage opendir for path: " << path_string
                                        << " returned with errno: " << saved_errno << std::endl;
        } else if(from_opendir) {
            LOG(WARNING) << "failed to passthrough readdir" << std::endl;
            LOG(ERROR) << "storage opendir for path: " << path_string << " returned with errno: " << saved_errno
                       << std::endl;
        } else if(saved_errno > 0 && saved_errno != ETIMEDOUT && saved_errno != EAGAIN && saved_errno != EIO) {
            // (offload timeouts/rejections/failures were logged by root_storage_reply)
            LOG(ERROR) << "storage readdir for path: " << path_string << " returned with errno: " << saved_errno
                       << std::endl;
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        path_request = CacheTables::tree_path_status_cache_table.begin(path_string);
        if(path_request->is_leader()) {
            LOG(DEBUG, RPC_READDIR_TAG) << "first to request path_string" << std::endl;
#endif
            try {
                const long long ready_deadline_ms = parent_ready_deadline_ms();
                // Lease first: keeps parent_endpoint alive across shutdown and is
                // destroyed after every endpoint/provider_handle copy below.
                const LeasedPtr<tl::endpoint>::Lease parent_lease(ServerLocalCacheProvider::parent_endpoint);
                const tl::endpoint& parent_endpoint = parent_lease.get();
                if(!ensure_parent_provider_ready(get_engine(), parent_endpoint, Node::parent_addr.value(), path_string,
                                                 ready_deadline_ms)) {
                    readdir_response = std::make_pair(-EIO, std::vector<std::string>(0));
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
                readdir_response = std::make_pair(-EIO, std::vector<std::string>(0));
            } catch(const std::exception& ex) {
                LOG(ERROR, RPC_READDIR_TAG) << "std::exception during parent rpc for path " << path_string
                                            << " via parent " << Node::parent_addr.value()
                                            << ": " << ex.what() << std::endl;
                readdir_response = std::make_pair(-EIO, std::vector<std::string>(0));
            }
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            LOG(DEBUG, RPC_READDIR_TAG) << "not first to request path_string, waiting on cache entry..." << std::endl;
            const int status = static_cast<int>(CacheTables::tree_path_status_cache_table.wait(*path_request));

            if(status != Constants::fs_operation_success) {
                readdir_response = std::make_pair(status, std::vector<std::string>(0));
            } else {
                const auto entry = CacheTables::tree_cache_table.get(path_string);
                readdir_response = entry ? std::make_pair(status, *entry)
                                         : std::make_pair(-EIO, std::vector<std::string>{});
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
    if(path_request && path_request->is_leader())
        CacheTables::tree_path_status_cache_table.complete(*path_request, readdir_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(readdir_response));
    } else {
        LOG(DEBUG, RPC_READDIR_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(readdir_response.first));
    }
}
