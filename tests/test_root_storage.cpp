// Root-node storage replies (review r7 findings 2 and 3).
//  1. RootStorage::call_reply turns every outcome into the handler's reply
//     type: done -> value; storage errno passes through; worker exception,
//     submission failure, timeout and rejection -> -EIO / -ETIMEDOUT / -EAGAIN.
//     It never throws, so a handler always reaches req.respond().
//  2. list_directory closes its DIR on every exit, including an exception
//     thrown while collecting entries (open fd count is unchanged).
#include "copper/root_storage.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

using Reply = std::pair<int, std::vector<std::string>>;
using CopperOffload::BlockingOffload;
using CopperOffload::Outcome;

// Pool whose first construction throws (as `new BlockingOffload` in
// storage_offload() could); a later attempt succeeds.
struct ThrowingPoolCtor {
    ThrowingPoolCtor() { if(fail) throw std::bad_alloc(); }
    BlockingOffload pool{1, 4};
    static inline bool fail = true;
};

// Capture whose second copy throws.
struct ThrowOnCopy {
    static inline int copies = 0;
    ThrowOnCopy() = default;
    ThrowOnCopy(const ThrowOnCopy&) {
        if(++copies >= 2) throw std::length_error("capture copy");
    }
};

static int open_fds() {
    int n = 0;
    DIR* d = opendir("/proc/self/fd");
    while(readdir(d) != nullptr) ++n;
    closedir(d);
    return n;
}

static void sleep_step(int) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

static Reply call(BlockingOffload& pool, std::function<Reply()> op, long long budget_ms, int* described) {
    return RootStorage::call_reply<Reply>(
        pool, op, CopperOffload::steady_now_ms() + budget_ms, sleep_step,
        [](int status) noexcept { return Reply(status, std::vector<std::string>{}); },
        [described](Outcome, int, const std::string&) {
            ++*described;
            throw std::runtime_error("logging failed");  // must be swallowed
        });
}

int main() {
    char tmpl[] = "/tmp/copper-root-storage-XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    for(int i = 0; i < 50; ++i) {
        FILE* f = std::fopen((dir + "/f" + std::to_string(i)).c_str(), "w");
        std::fclose(f);
    }

    BlockingOffload pool(2, 8);
    int described = 0;

    // done: listing succeeds
    Reply r = call(pool, [&dir] {
        auto l = RootStorage::list_directory(dir);
        return Reply(l.status, std::move(l.entries));
    }, 2000, &described);
    CHECK(r.first == 0 && r.second.size() == 52 && described == 0);  // 50 files + . + ..

    // storage errno passes through unchanged (not an offload failure)
    r = call(pool, [] {
        auto l = RootStorage::list_directory("/nonexistent/copper/dir");
        CHECK(l.opendir_failed);
        return Reply(l.status, std::move(l.entries));
    }, 2000, &described);
    CHECK(r.first == -ENOENT && r.second.empty() && described == 0);

    // worker exception (e.g. allocation while collecting entries) -> -EIO reply
    r = call(pool, []() -> Reply { throw std::bad_alloc(); }, 2000, &described);
    CHECK(r.first == -EIO && r.second.empty() && described == 1);

    // submission failure -> -EIO reply
    {
        BlockingOffload broken(1, 4);
        broken.set_submit_hook_for_test([] { throw std::runtime_error("cannot create worker"); });
        r = call(broken, [] { return Reply(0, {}); }, 2000, &described);
        CHECK(r.first == -EIO && described == 2);
    }

    // timeout -> -ETIMEDOUT reply within the budget
    {
        const long long t = CopperOffload::steady_now_ms();
        r = call(pool, [] { std::this_thread::sleep_for(std::chrono::milliseconds(300)); return Reply(0, {"late"}); },
                 50, &described);
        const long long dt = CopperOffload::steady_now_ms() - t;
        CHECK(r.first == -ETIMEDOUT && r.second.empty() && described == 3);
        CHECK(dt >= 50 && dt < 200);
    }

    // rejection (queue full, workers busy) -> -EAGAIN reply
    {
        BlockingOffload tiny(1, 0);  // no queue slots at all
        r = call(tiny, [] { return Reply(0, {}); }, 2000, &described);
        CHECK(r.first == -EAGAIN && described == 4);
    }
    std::printf("call_reply: done, errno passthrough, exception->EIO, submit failure->EIO, timeout->ETIMEDOUT, "
                "reject->EAGAIN; describe() exceptions swallowed\n");

    // ---- guarded_reply (review r8 finding 1): failures while PREPARING the
    // call - pool acquisition on first use, building the closure (path copy,
    // shared flag), copying a capture into std::function - become -EIO
    // replies; nothing escapes, and nothing is submitted.
    {
        auto make_failure = [](int status) noexcept { return Reply(status, std::vector<std::string>{}); };
        auto describe = [](Outcome, int, const std::string&) {};
        int prepare_failures = 0;
        std::string last_error;
        auto on_prepare = [&](const char* e) {
            ++prepare_failures;
            last_error = e;
            throw std::runtime_error("logging failed");  // must be swallowed
        };
        auto deadline = [] { return CopperOffload::steady_now_ms() + 2000; };

        // (a) first-use pool construction throws, as storage_offload() would
        // if `new BlockingOffload` failed; a later call constructs it.
        auto get_pool = []() -> BlockingOffload& {
            static ThrowingPoolCtor holder;  // function-local static: retried after a throw
            return holder.pool;
        };
        int ran = 0;
        auto make_op = [&ran] { return [&ran] { ++ran; return Reply(0, {"ok"}); }; };
        Reply r = RootStorage::guarded_reply<Reply>(get_pool, make_op, deadline(), sleep_step, make_failure,
                                                    describe, on_prepare);
        CHECK(r.first == -EIO && r.second.empty() && ran == 0 && prepare_failures == 1);
        ThrowingPoolCtor::fail = false;
        r = RootStorage::guarded_reply<Reply>(get_pool, make_op, deadline(), sleep_step, make_failure, describe,
                                              on_prepare);
        CHECK(r.first == 0 && r.second.size() == 1 && ran == 1 && prepare_failures == 1);

        // (b) building the closure throws (path copy / make_shared flag)
        auto pool_ok = [&pool]() -> BlockingOffload& { return pool; };
        auto make_op_throws = []() -> std::function<Reply()> { throw std::bad_alloc(); };
        r = RootStorage::guarded_reply<Reply>(pool_ok, make_op_throws, deadline(), sleep_step, make_failure,
                                              describe, on_prepare);
        CHECK(r.first == -EIO && prepare_failures == 2);

        // (c) copying a capture into std::function throws
        // The first copy (the lambda capture inside make_op) succeeds; the
        // second - when guarded_reply converts the returned closure into a
        // std::function - throws.
        auto make_op_copy_throws = [] {
            ThrowOnCopy t;
            return [t]() { return Reply(0, {}); };
        };
        r = RootStorage::guarded_reply<Reply>(pool_ok, make_op_copy_throws, deadline(), sleep_step, make_failure,
                                              describe, on_prepare);
        CHECK(r.first == -EIO && prepare_failures == 3 && last_error == "capture copy" && ThrowOnCopy::copies >= 2);

        // (d) a non-std exception during preparation
        auto make_op_int = []() -> std::function<Reply()> { throw 42; };
        r = RootStorage::guarded_reply<Reply>(pool_ok, make_op_int, deadline(), sleep_step, make_failure,
                                              describe, on_prepare);
        CHECK(r.first == -EIO && prepare_failures == 4);
        std::printf("guarded_reply: pool first-use failure, closure build failure, capture-copy failure, "
                    "non-std exception -> -EIO (4/4), retry after pool failure -> ok\n");
    }

    // DIR is closed when collecting entries throws
    const int before = open_fds();
    for(int i = 0; i < 20; ++i) {
        bool thrown = false;
        try {
            int added = 0;
            RootStorage::list_directory_with(dir, [&added](std::vector<std::string>& v, const char* name) {
                if(++added == 10) throw std::bad_alloc();
                v.emplace_back(name);
            });
        } catch(const std::bad_alloc&) {
            thrown = true;
        }
        CHECK(thrown);
    }
    const int after = open_fds();
    CHECK(after == before);
    std::printf("list_directory: 20 throwing listings, open fds %d -> %d\n", before, after);

    std::printf("PASS: root storage replies are total and release directory handles\n");
    return 0;
}
