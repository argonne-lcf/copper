// Storage-call offload on ONE execution stream (cu_fuse default -es 1).
// Each "handler" ULT makes a blocking call through BlockingOffload, waiting
// with cooperative yields (as storage_wait_step does with tl::thread). The
// blocking calls really block their OS thread (usleep / a call that never
// returns), so if the handler ran the call inline the single stream would be
// parked:
//  A: 8 handlers each do a 200 ms blocking call (4 workers). An unrelated
//     handler ULT keeps ticking (max gap small), all 8 complete, total time
//     ~= 2 rounds of 200 ms (parallel on workers, not serialized on the stream).
//  B: a call that hangs forever times out at the caller's deadline with
//     Outcome::timed_out; the stream stays live; later calls still complete on
//     the remaining workers.
//  C: when every worker hangs and the queue is full, new calls are rejected
//     immediately (bounded memory, no unbounded wait).
//  D: the result value and exceptions are returned to the caller intact.
// Inline execution (the pre-fix handler code) parks the stream -> alarm -> rc 2.
#include "fs/blocking_offload.h"
#include "abt_test_support.h"

#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using CopperOffload::BlockingOffload;
using CopperOffload::Outcome;

static std::atomic<int> handlers_done{0}, other_ticks{0}, completed{0};
static std::atomic<long long> max_gap_ms{0};
static std::atomic<bool> hang_release{false};

static void yield_step(int) { ABT_OK(ABT_thread_yield()); }

#ifdef COPPER_OFFLOAD_TEST_INLINE
// Mutation hook: what the handlers did before the fix (call inline).
template <typename R>
static Outcome offload_run(BlockingOffload&, const std::function<R()>& fn, std::optional<R>& out, long long) {
    out.emplace(fn());
    return Outcome::done;
}
#else
template <typename R>
static Outcome offload_run(BlockingOffload& pool, const std::function<R()>& fn, std::optional<R>& out, long long deadline) {
    return pool.run<R>(fn, out, deadline, yield_step);
}
#endif

struct HandlerArg {
    BlockingOffload* pool;
    int block_ms;
    long long budget_ms;
    Outcome outcome;
    int value;
    int delay_ms = 0;  // cooperative delay before submitting (orders submissions)
};

static void handler(void* p) {
    auto* a = static_cast<HandlerArg*>(p);
    if(a->delay_ms > 0) abt_sleep_ms(a->delay_ms);
    std::optional<int> out;
    const int ms = a->block_ms;
    a->outcome = offload_run<int>(*a->pool, [ms]() {
        if(ms < 0) {
            while(!hang_release.load()) usleep(1000);  // hung storage call
            return -1;
        }
        usleep(ms * 1000);  // blocks this OS thread, like lstat on a slow OST
        return ms;
    }, out, now_ms() + a->budget_ms);
    if(a->outcome == Outcome::done) { a->value = *out; completed++; }
    handlers_done++;
}

static void other_handler(void* n) {
    const int target = *static_cast<int*>(n);
    long long last = now_ms();
    while(handlers_done.load() < target) {
        other_ticks++;
        const long long t = now_ms();
        if(t - last > max_gap_ms) max_gap_ms = t - last;
        last = t;
        ABT_OK(ABT_thread_yield());
    }
}

static void run_handlers(ABT_pool pool, std::vector<HandlerArg>& args) {
    handlers_done = 0; other_ticks = 0; max_gap_ms = 0;
    int n = static_cast<int>(args.size());
    std::vector<ABT_thread> t(args.size());
    ABT_thread o;
    ABT_OK(ABT_thread_create(pool, other_handler, &n, ABT_THREAD_ATTR_NULL, &o));
    for(std::size_t i = 0; i < args.size(); ++i)
        ABT_OK(ABT_thread_create(pool, handler, &args[i], ABT_THREAD_ATTR_NULL, &t[i]));
    for(auto& th : t) ABT_OK(ABT_thread_free(&th));
    ABT_OK(ABT_thread_free(&o));
}

int main() {
    std::signal(SIGALRM, on_alarm);
    alarm(20);
    ABT_OK(ABT_init(0, nullptr));
    ABT_pool pool = single_stream_pool();

    // ---- A: parallel blocking calls, stream stays live
    {
        BlockingOffload off(4, 64);
        std::vector<HandlerArg> args(8, HandlerArg{&off, 200, 5000, Outcome::rejected, 0});
        completed = 0;
        const long long t0 = now_ms();
        run_handlers(pool, args);
        const long long dt = now_ms() - t0;
        for(auto& a : args) CHECK(a.outcome == Outcome::done && a.value == 200);
        CHECK(completed == 8);
        CHECK(other_ticks > 100);
        CHECK(max_gap_ms < 25);              // inline: 200 ms per call
        CHECK(dt >= 390 && dt < 1200);       // 8 calls / 4 workers = 2 rounds; inline serial = 1600 ms
        CHECK(off.started_workers() <= 4);
        std::printf("case A: 8 x 200 ms calls on 4 workers in %lld ms; other handler ticks=%d max_gap_ms=%lld\n",
                    dt, other_ticks.load(), max_gap_ms.load());
    }

    // ---- B: a hung call times out at the deadline; others still complete
    BlockingOffload off(2, 2);  // kept alive (workers stay hung until release)
    {
        std::vector<HandlerArg> args{{&off, -1, 300, Outcome::rejected, 0},   // hangs
                                     {&off, 50, 2000, Outcome::rejected, 0}}; // normal
        completed = 0;
        const long long t0 = now_ms();
        run_handlers(pool, args);
        const long long dt = now_ms() - t0;
        CHECK(args[0].outcome == Outcome::timed_out);
        CHECK(args[1].outcome == Outcome::done && args[1].value == 50);
        CHECK(dt >= 290 && dt < 600);        // bounded by the 300 ms budget
        CHECK(max_gap_ms < 25);
        CHECK(off.timed_out_total() == 1);
        std::printf("case B: hung call timed out after %lld ms (budget 300), concurrent call completed, max_gap_ms=%lld\n",
                    dt, max_gap_ms.load());
    }

    // ---- C: all workers hung + queue full -> immediate rejection.
    // Fresh pool: 1 worker, queue capacity 1. h0 (t=0) takes the worker and
    // hangs; h1 (t=20 ms) fills the queue; the late call (t=50 ms) must be
    // rejected at once. Submissions are staggered so the result is deterministic.
    BlockingOffload c(1, 1);
    {
        std::vector<HandlerArg> hang{{&c, -1, 400, Outcome::rejected, 0, 0},
                                     {&c, -1, 400, Outcome::rejected, 0, 20}};
        static long long reject_ms = -1;
        static Outcome late_outcome = Outcome::done;
        struct L {
            static void late(void* p) {
                abt_sleep_ms(50);
                auto* o = static_cast<BlockingOffload*>(p);
                std::optional<int> out;
                const long long t = now_ms();
                late_outcome = offload_run<int>(*o, []() { return 7; }, out, now_ms() + 5000);
                reject_ms = now_ms() - t;
            }
        };
        ABT_thread lt;
        ABT_OK(ABT_thread_create(pool, L::late, &c, ABT_THREAD_ATTR_NULL, &lt));
        run_handlers(pool, hang);
        ABT_OK(ABT_thread_free(&lt));
        CHECK(hang[0].outcome == Outcome::timed_out);  // running on the hung worker
        CHECK(hang[1].outcome == Outcome::timed_out);  // queued behind it, never ran
        CHECK(late_outcome == Outcome::rejected);
        CHECK(reject_ms >= 0 && reject_ms < 20);
        CHECK(c.started_workers() == 1);    // never more threads than configured
        CHECK(c.rejected_total() == 1);
        std::printf("case C: worker hung, queue full -> late call rejected in %lld ms; workers=%llu rejected=%llu\n",
                    reject_ms, c.started_workers(), c.rejected_total());
    }
    hang_release = true;  // let the hung workers finish

    // ---- D: values come back intact; exceptions become Outcome::failed (no throw)
    {
        BlockingOffload d(1, 4);
        std::optional<std::string> s;
        CHECK(d.run<std::string>([] { return std::string(1 << 20, 'x'); }, s, now_ms() + 2000, yield_step) == Outcome::done);
        CHECK(s && s->size() == (1u << 20));
        std::optional<int> v;
        std::string msg;
        const Outcome o = d.run<int>([]() -> int { throw std::runtime_error("boom"); }, v, now_ms() + 2000, yield_step, &msg);
        CHECK(o == Outcome::failed && !v && msg == "boom" && d.failed_total() == 1);
        CHECK(CopperOffload::status_of(o) == -EIO);
        std::printf("case D: 1 MiB result returned; worker exception -> failed(\"%s\") without throwing\n", msg.c_str());
    }

    // ---- E (review r7 finding 1): pre-expired deadline -> timed_out, nothing submitted
    {
        BlockingOffload e(1, 4);
        std::atomic<int> ran{0};
        std::optional<int> v;
        const Outcome o = e.run<int>([&ran] { ran++; return 1; }, v, now_ms() - 1, yield_step);
        usleep(20 * 1000);
        CHECK(o == Outcome::timed_out && !v && ran == 0 && e.started_workers() == 0);
        std::printf("case E: pre-expired deadline -> timed_out, call never started, 0 workers\n");
    }

    // ---- F (review r7 finding 1, reviewer's reproduction): worker finishes AFTER
    // the deadline while the caller is descheduled -> timed_out, not done.
    // budget 10 ms, worker 20 ms, caller's wait step sleeps 60 ms.
    {
        BlockingOffload f(1, 4);
        std::optional<int> v;
        const long long t = now_ms();
        const Outcome o = f.run<int>([] { usleep(20 * 1000); return 7; }, v, t + 10,
                                     [](int) { abt_sleep_ms(60); });
        const long long dt = now_ms() - t;
        CHECK(o == Outcome::timed_out && !v);
        // F2: completion inside the budget but observed late is still done.
        std::optional<int> w;
        const long long t2 = now_ms();
        const Outcome o2 = f.run<int>([] { usleep(5 * 1000); return 8; }, w, t2 + 30,
                                      [](int) { abt_sleep_ms(60); });
        CHECK(o2 == Outcome::done && w && *w == 8);
        std::printf("case F: late completion (20 ms > budget 10 ms, observed at %lld ms) -> timed_out; "
                    "in-budget completion observed late -> done\n", dt);
    }

    // ---- G (review r7 finding 2): a failure during submission -> failed, no throw
    {
        BlockingOffload g(1, 4);
        g.set_submit_hook_for_test([] { throw std::bad_alloc(); });
        std::optional<int> v;
        std::string msg;
        const Outcome o = g.run<int>([] { return 1; }, v, now_ms() + 1000, yield_step, &msg);
        CHECK(o == Outcome::failed && !v && !msg.empty() && g.started_workers() == 0);
        std::printf("case G: submission failure (%s) -> failed without throwing\n", msg.c_str());
    }

    std::printf("PASS: blocking offload keeps the single stream live and bounds storage calls\n");
    std::fflush(stdout);
    _exit(0);  // ABT_finalize would wait on nothing, but keep exit independent of worker state
}
