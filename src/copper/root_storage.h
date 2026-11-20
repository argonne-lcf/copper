#ifndef COPPER_ROOT_STORAGE_H
#define COPPER_ROOT_STORAGE_H

// Root-node storage access for the RPC handlers (rpcLstat / rpcRead /
// rpcReaddir when this node is the tree root). Each call runs on the
// BlockingOffload pool and ALWAYS yields a reply value of the handler's reply
// type: success, the storage errno, or -ETIMEDOUT / -EAGAIN / -EIO when the
// call timed out, could not be queued, or failed (review r7 finding 2). No
// exception leaves call_reply(), so a handler always reaches req.respond().
//
// Directory listing owns its DIR* with RAII so an exception while collecting
// entries (allocation) still closes it (review r7 finding 3).

#include <dirent.h>

#include <cerrno>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../fs/blocking_offload.h"

namespace RootStorage {

// Result of listing a directory on a worker thread.
struct DirListing {
    int status = 0;               // 0 or -errno
    bool opendir_failed = false;  // status came from opendir (vs readdir)
    std::vector<std::string> entries;
};

struct DirCloser {
    void operator()(DIR* d) const noexcept {
        if(d != nullptr) closedir(d);
    }
};
using DirHandle = std::unique_ptr<DIR, DirCloser>;

// opendir/readdir/closedir on the calling thread. `add` appends one entry and
// may throw (allocation); the DIR is closed on every exit path.
template <typename Add>
DirListing list_directory_with(const std::string& path, Add&& add) {
    DirListing r;
    DirHandle dir(opendir(path.c_str()));
    if(!dir) {
        r.status = -errno;
        r.opendir_failed = true;
        return r;
    }
    errno = 0;
    while(const dirent* de = readdir(dir.get())) {
        add(r.entries, de->d_name);
        errno = 0;
    }
    if(errno != 0) {
        r.status = -errno;
        r.entries.clear();
    }
    return r;
}

inline DirListing list_directory(const std::string& path) {
    return list_directory_with(path, [](std::vector<std::string>& v, const char* name) { v.emplace_back(name); });
}

// Runs `op` on `pool` with the given deadline and converts every outcome into
// a Reply. `make_failure(status)` builds the reply for a negative status and
// must not throw (pair of an integer and an empty vector does not allocate).
// `describe(outcome, status, message)` is called for non-done outcomes, for
// logging; exceptions from it are swallowed.
template <typename Reply, typename MakeFailure, typename Describe>
Reply call_reply(CopperOffload::BlockingOffload& pool, const std::function<Reply()>& op, long long deadline_ms,
                 const std::function<void(int)>& wait_step, MakeFailure&& make_failure, Describe&& describe) noexcept {
    std::optional<Reply> result;
    std::string message;
    const auto outcome = pool.run<Reply>(op, result, deadline_ms, wait_step, &message);
    if(outcome == CopperOffload::Outcome::done && result.has_value()) return std::move(*result);
    const int status = outcome == CopperOffload::Outcome::done ? -EIO : CopperOffload::status_of(outcome);
    try {
        describe(outcome, status, message);
    } catch(...) {
    }
    return make_failure(status);
}

// The production entry point (review r8 finding 1). Everything that can
// allocate or throw while PREPARING the call happens inside one try/catch:
// acquiring the pool (`get_pool`, which may construct it on first use),
// building the worker closure (`make_op`, which copies the path and any
// shared state the handler reads afterwards) and the std::function
// conversions inside call_reply. Any exception there yields
// make_failure(-EIO). `make_failure` must not throw. GetPool, MakeOp,
// WaitStep and Describe are taken by template so passing them does not
// allocate before the try block is entered.
template <typename Reply, typename GetPool, typename MakeOp, typename WaitStep, typename MakeFailure,
          typename Describe, typename Prepare>
Reply guarded_reply(GetPool&& get_pool, MakeOp&& make_op, long long deadline_ms, WaitStep&& wait_step,
                    MakeFailure&& make_failure, Describe&& describe, Prepare&& on_prepare_failure) noexcept {
    try {
        CopperOffload::BlockingOffload& pool = get_pool();
        const std::function<Reply()> op = make_op();
        const std::function<void(int)> step = wait_step;
        return call_reply<Reply>(pool, op, deadline_ms, step, make_failure, describe);
    } catch(const std::exception& e) {
        try {
            on_prepare_failure(e.what());
        } catch(...) {
        }
    } catch(...) {
        try {
            on_prepare_failure("non-standard exception");
        } catch(...) {
        }
    }
    return make_failure(-EIO);
}

}  // namespace RootStorage

#endif  // COPPER_ROOT_STORAGE_H
