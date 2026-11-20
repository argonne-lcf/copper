#ifndef PATH_STATUS_CACHE_H
#define PATH_STATUS_CACHE_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <stddef.h>
#include <sys/types.h>

#include "abt_sync.h"

// Coalesces concurrent parent RPCs for the same path (BLOCK_REDUNDANT_RPCS).
// Waiters run as RPC handler ULTs, so the lock and wait are Argobots-aware.
// The previous std::condition_variable wait parked the only handler execution
// stream (-es 1); gdb stacks of the 12-node -T 1 hang (job 9105916) show the
// handlers blocked in pthread_cond_wait under PathStatusCache::wait().
class PathStatusCache final {
    class State;
    public:
    using Key = std::string;
    class Request {
    public:
        bool is_leader() const { return leader_; }
    private:
        Request(Key key, std::shared_ptr<State> state, bool leader)
            : key_(std::move(key)), state_(std::move(state)), leader_(leader) {}
        Key key_;
        std::shared_ptr<State> state_;
        bool leader_ = false;
        friend class PathStatusCache;
    };

    Request begin(const Key& key);
    void complete(const Request& request, ssize_t status);
    ssize_t wait(const Request& request);
    size_t active_size() const;


    private:
    class State {
    public:
        bool complete = false;
        ssize_t status = 0;  // full width: rpcRead merges byte counts > INT_MAX
    };
    std::unordered_map<Key, std::shared_ptr<State>> active_;
    mutable AbtMutex mutex_;
    AbtCondition cv_;
};

#endif
