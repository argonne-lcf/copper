// Regression test for the BLOCK_REDUNDANT_RPCS deadlock (12 nodes, -T 1, job 9105916).
// Leader and waiter for one path run as ULTs on ONE execution stream, the shape
// of cu_fuse's default -es 1. The leader does not complete() until the waiter
// has provably entered PathStatusCache::wait() (handshake), so the test does not
// depend on pool scheduling order. A wait that blocks the OS thread
// (std::condition_variable) can never let the leader run -> alarm -> rc 2.
#include "cache/path_status_cache.h"
#include "abt_test_support.h"

static PathStatusCache cache;
struct Round { PathStatusCache::Request* leader; PathStatusCache::Request* waiter; ssize_t status; ssize_t result; std::atomic<int> waiter_entered; std::atomic<int> leader_done; };
static Round* cur;

static void waiter_fn(void*) {
    cur->waiter_entered.store(1);
    cur->result = cache.wait(*cur->waiter);  // must suspend here: leader has not completed
}
static void leader_fn(void*) {
    while(!cur->waiter_entered.load()) ABT_OK(ABT_thread_yield());
    // Give the waiter several turns so it is parked inside wait(), not merely started.
    for(int i = 0; i < 64; ++i) ABT_OK(ABT_thread_yield());
    CHECK(cur->result == 12345);              // waiter really is still waiting
    cache.complete(*cur->leader, cur->status);
    cur->leader_done.store(1);
}

int main() {
    std::signal(SIGALRM, on_alarm);
    alarm(10);
    ABT_OK(ABT_init(0, nullptr));
    ABT_pool pool = single_stream_pool();

    // includes a successful byte count above INT_MAX (merged rpcRead status)
    const ssize_t statuses[] = {-ENOENT, 0, 4096, static_cast<ssize_t>(INT_MAX) + 1, 5LL * 1024 * 1024 * 1024};
    for(int order = 0; order < 2; ++order) {          // both enqueue orders
        for(ssize_t st : statuses) {
            auto l = cache.begin("/lus/flare/projects/Performance");
            auto w = cache.begin("/lus/flare/projects/Performance");
            CHECK(l.is_leader() && !w.is_leader());
            Round r{&l, &w, st, 12345, {0}, {0}};
            cur = &r;
            ABT_thread t1, t2;
            if(order == 0) {
                ABT_OK(ABT_thread_create(pool, waiter_fn, nullptr, ABT_THREAD_ATTR_NULL, &t1));
                ABT_OK(ABT_thread_create(pool, leader_fn, nullptr, ABT_THREAD_ATTR_NULL, &t2));
            } else {
                ABT_OK(ABT_thread_create(pool, leader_fn, nullptr, ABT_THREAD_ATTR_NULL, &t2));
                ABT_OK(ABT_thread_create(pool, waiter_fn, nullptr, ABT_THREAD_ATTR_NULL, &t1));
            }
            ABT_OK(ABT_thread_free(&t1));
            ABT_OK(ABT_thread_free(&t2));
            CHECK(r.leader_done.load() == 1);
            if(r.result != st) { std::fprintf(stderr, "FAIL: status %lld propagated as %lld\n", (long long)st, (long long)r.result); return 1; }
            CHECK(cache.active_size() == 0);
        }
    }
    ABT_OK(ABT_finalize());
    std::printf("PASS: waiter suspended in wait() on a single execution stream; statuses incl. > INT_MAX propagate exactly\n");
    return 0;
}
