// Parent-readiness gate on ONE execution stream (cu_fuse default -es 1).
// Case A (no-merge startup shape): several handler ULTs need the parent while
//   it is not ready yet; the probe and back-off yield. An unrelated "other
//   handler" ULT must keep making progress, and all callers succeed once the
//   parent becomes ready. An OS-blocking back-off (std::this_thread::sleep_for)
//   parks the stream -> alarm -> rc 2. (The gate is acquired by cooperative
//   try_lock polling, so swapping AbtMutex for std::mutex alone does not
//   deadlock here; AbtMutex is kept so any future blocking lock() stays safe.)
// Case B (non-responding parent): every probe times out. Callers must return
//   false within their deadline (no indefinite wait), queued callers fail via
//   shared failure instead of each starting a full new wait, and the next
//   wait after the failure starts fresh.
#include "cache/parent_readiness.h"
#include "abt_test_support.h"
#include <limits>

static ParentReadiness::Gate gate;
static ParentReadiness::Config cfg;
static std::atomic<long long> parent_ready_at{0};
static std::atomic<int> probes{0}, ok{0}, failed{0}, shared{0}, other_ticks{0};
static std::atomic<int> callers_done{0};
static bool nonresponding = false;
static long long max_return_ms = 0, t0 = 0, max_gap_ms = 0;

static ParentReadiness::Hooks hooks() {
    ParentReadiness::Hooks h;
    h.probe = [](long long timeout_ms) {
        probes++;
        if(nonresponding) { abt_sleep_ms(timeout_ms); return ParentReadiness::Probe::error; }  // RPC timeout
        abt_sleep_ms(2);                                                                       // RPC latency
        return now_ms() >= parent_ready_at.load() ? ParentReadiness::Probe::ready : ParentReadiness::Probe::not_ready;
    };
    h.sleep_ms = [](long long ms) { abt_sleep_ms(ms); };
    h.on_shared_failure = [] { shared++; };
    return h;
}

static void caller(void*) {
    const bool r = ParentReadiness::ensure(gate, cfg, hooks());
    (r ? ok : failed)++;
    const long long dt = now_ms() - t0;
    if(dt > max_return_ms) max_return_ms = dt;
    callers_done++;
}
static void other_handler(void* n) {
    const int callers = *static_cast<int*>(n);
    long long last = now_ms();
    while(callers_done.load() < callers) {
        other_ticks++;
        const long long t = now_ms();
        if(t - last > max_gap_ms) max_gap_ms = t - last;  // how long the stream was taken from us
        last = t;
        ABT_OK(ABT_thread_yield());
    }
}

static void run(ABT_pool pool, int callers) {
    callers_done = 0;
    ABT_thread t[16], o;
    ABT_OK(ABT_thread_create(pool, other_handler, &callers, ABT_THREAD_ATTR_NULL, &o));
    for(int i = 0; i < callers; ++i) ABT_OK(ABT_thread_create(pool, caller, nullptr, ABT_THREAD_ATTR_NULL, &t[i]));
    for(int i = 0; i < callers; ++i) ABT_OK(ABT_thread_free(&t[i]));
    ABT_OK(ABT_thread_free(&o));
}

int main() {
    std::signal(SIGALRM, on_alarm);
    alarm(20);
    ABT_OK(ABT_init(0, nullptr));
    ABT_pool pool = single_stream_pool();

    // ---- Case A: parent becomes ready after 300 ms
    cfg.timeout_ms = 5000; cfg.max_probe_timeout_ms = 200; cfg.initial_retry_sleep_ms = 10; cfg.max_retry_sleep_ms = 50;
    t0 = now_ms(); parent_ready_at = t0 + 300;
    run(pool, 8);
    CHECK(ok == 8 && failed == 0 && gate.ready());
    CHECK(other_ticks > 100);                   // stream stayed live while callers waited
    CHECK(max_gap_ms < 25);                     // back-off (10..50 ms) yields; an OS sleep starves us for its full length
    CHECK(max_return_ms >= 300 && max_return_ms < 2000);
    const int probes_a = probes.load();
    CHECK(probes_a >= 2 && probes_a < 60);      // one prober, not 8 concurrent probe storms
    std::printf("case A: 8 callers ok after %lld ms, %d probes, other handler ticks=%d max_gap_ms=%lld\n", max_return_ms, probes_a, other_ticks.load(), max_gap_ms);

    // ---- Case B: parent never answers
    gate.reset(); ok = failed = shared = probes = other_ticks = 0; max_return_ms = 0; max_gap_ms = 0;
    nonresponding = true;
    cfg.timeout_ms = 400; cfg.max_probe_timeout_ms = 100;
    t0 = now_ms();
    run(pool, 6);
    CHECK(ok == 0 && failed == 6 && !gate.ready());
    CHECK(max_return_ms < 400 + 60);             // every caller bounded by ITS budget, not 6 x 400 ms serialized
    CHECK(other_ticks > 100);
    CHECK(max_gap_ms < 25);
    std::printf("case B: 6 callers failed in %lld ms (budget 400), shared_failures=%d, probes=%d\n", max_return_ms, shared.load(), probes.load());

    // ---- Case B2: staggered caller with a later deadline. It is queued behind a
    // holder whose wait then fails; it must fail immediately (shared failure)
    // rather than start its own probing.
    gate.reset(); shared = 0; nonresponding = true;
    {
        static long long late_elapsed = -1; static int late_ok = -1; static std::atomic<int> b2_done{0};
        b2_done = 0;
        struct L {
            static void holder(void*) {
                ParentReadiness::Config c; c.timeout_ms = 300; c.max_probe_timeout_ms = 100;
                CHECK(!ParentReadiness::ensure(gate, c, hooks()));
                b2_done++;
            }
            static void late(void*) {
                abt_sleep_ms(100);  // arrives while the holder is probing
                ParentReadiness::Config c; c.timeout_ms = 1000; c.max_probe_timeout_ms = 100;
                const long long t = now_ms();
                late_ok = ParentReadiness::ensure(gate, c, hooks()) ? 1 : 0;
                late_elapsed = now_ms() - t;
                b2_done++;
            }
        };
        ABT_thread a, b;
        ABT_OK(ABT_thread_create(pool, L::holder, nullptr, ABT_THREAD_ATTR_NULL, &a));
        ABT_OK(ABT_thread_create(pool, L::late, nullptr, ABT_THREAD_ATTR_NULL, &b));
        ABT_OK(ABT_thread_free(&a)); ABT_OK(ABT_thread_free(&b));
        CHECK(b2_done == 2 && late_ok == 0 && shared >= 1);
        CHECK(late_elapsed < 300);  // ~200 ms (holder's remaining time), not its own 1000 ms budget
        std::printf("case B2: late caller (budget 1000) failed via shared failure after %lld ms\n", late_elapsed);
    }

    // ---- after a failure, a new request gets a fresh bounded wait and can succeed
    nonresponding = false; parent_ready_at = 0; ok = failed = 0; cfg.timeout_ms = 1000;
    t0 = now_ms();
    run(pool, 1);
    CHECK(ok == 1 && gate.ready());

    // ---- Case C (review r2 finding 1): strict per-caller deadlines.
    // C1: a caller queued behind a slow prober returns false by ITS OWN deadline
    //     (gate acquisition is deadline-aware), even though the prober later succeeds.
    gate.reset(); nonresponding = false;
    {
        static long long slow_probe_ms = 300, queued_returned_at = -1, queued_t0 = 0; static int queued_ok = -1;
        static std::atomic<int> c_done{0};
        c_done = 0;
        struct L {
            static void prober(void*) {
                ParentReadiness::Config c; c.timeout_ms = 2000; c.max_probe_timeout_ms = 1000;
                ParentReadiness::Hooks h = hooks();
                h.probe = [](long long) { abt_sleep_ms(slow_probe_ms); return ParentReadiness::Probe::ready; };
                CHECK(ParentReadiness::ensure(gate, c, h));
                c_done++;
            }
            static void queued(void*) {
                ABT_OK(ABT_thread_yield());  // let the prober take the gate first
                ParentReadiness::Config c; c.timeout_ms = 50;
                queued_t0 = now_ms();
                queued_ok = ParentReadiness::ensure(gate, c, hooks()) ? 1 : 0;
                queued_returned_at = now_ms() - queued_t0;
                c_done++;
            }
        };
        ABT_thread a, b;
        ABT_OK(ABT_thread_create(pool, L::prober, nullptr, ABT_THREAD_ATTR_NULL, &a));
        ABT_OK(ABT_thread_create(pool, L::queued, nullptr, ABT_THREAD_ATTR_NULL, &b));
        ABT_OK(ABT_thread_free(&a)); ABT_OK(ABT_thread_free(&b));
        CHECK(c_done == 2 && gate.ready());
        CHECK(queued_ok == 0);
        CHECK(queued_returned_at >= 50 && queued_returned_at < 50 + 20);
        std::printf("case C1: queued caller (budget 50 ms) returned false after %lld ms while prober held gate %lld ms\n",
                    queued_returned_at, slow_probe_ms);
    }
    // C2: a ready answer that arrives after the caller's deadline is not success
    //     for that caller, but the gate records the parent as ready.
    gate.reset();
    {
        ParentReadiness::Config c; c.timeout_ms = 10; c.max_probe_timeout_ms = 1000;
        ParentReadiness::Hooks h = hooks();
        h.probe = [](long long) { abt_sleep_ms(50); return ParentReadiness::Probe::ready; };
        const long long t = now_ms();
        CHECK(!ParentReadiness::ensure(gate, c, h));
        CHECK(now_ms() - t < 50 + 20);
        CHECK(gate.ready());
        std::printf("case C2: late ready (50 ms > budget 10 ms) rejected for caller, gate ready\n");
    }
    // C3: an absolute deadline that was already spent before ensure() is
    //     honoured while the gate is not ready.
    gate.reset();
    {
        ParentReadiness::Config c; c.deadline_ms = now_ms() + 30; c.timeout_ms = 100000;
        abt_sleep_ms(40);  // earlier handler work consumed the whole budget
        int probed = 0;
        ParentReadiness::Hooks h = hooks();
        h.probe = [&probed](long long) { probed++; return ParentReadiness::Probe::ready; };
        CHECK(!ParentReadiness::ensure(gate, c, h));
        CHECK(probed == 0);
        std::printf("case C3: budget spent before ensure() -> false with 0 probes\n");
    }

    // C4 (review r3 finding 1): budget already spent and the gate IS ready.
    //     The deadline is checked before the ready fast path -> false.
    gate.reset();
    {
        ParentReadiness::Config ok; ok.timeout_ms = 1000;
        ParentReadiness::Hooks h = hooks();
        h.probe = [](long long) { return ParentReadiness::Probe::ready; };
        CHECK(ParentReadiness::ensure(gate, ok, h));
        CHECK(gate.ready());
        ParentReadiness::Config late; late.deadline_ms = now_ms() - 1;
        int probed = 0;
        h.probe = [&probed](long long) { probed++; return ParentReadiness::Probe::ready; };
        CHECK(!ParentReadiness::ensure(gate, late, h));
        CHECK(probed == 0);
        ParentReadiness::Config fresh; fresh.timeout_ms = 1000;
        CHECK(ParentReadiness::ensure(gate, fresh, h));  // in-budget caller still uses the fast path
        CHECK(probed == 0);
        std::printf("case C4: expired budget rejected even with ready gate; in-budget caller fast-paths\n");
    }
    // C5: invalid public Config values are sanitized (no zero/negative sleep
    //     loops, no signed overflow in deadline/backoff arithmetic).
    gate.reset();
    {
        ParentReadiness::Config c; c.timeout_ms = std::numeric_limits<long long>::max();
        c.initial_retry_sleep_ms = 0; c.max_retry_sleep_ms = -5; c.gate_poll_ms = 0; c.max_probe_timeout_ms = 0;
        int probes = 0;
        ParentReadiness::Hooks h = hooks();
        h.probe = [&probes](long long t) { CHECK(t >= 1); return ++probes >= 3 ? ParentReadiness::Probe::ready
                                                                                : ParentReadiness::Probe::not_ready; };
        CHECK(ParentReadiness::ensure(gate, c, h));
        CHECK(probes == 3);
        CHECK(ParentReadiness::retry_sleep_ms(ParentReadiness::sanitize(c), 1000) >= 1);
        ParentReadiness::Config big; big.initial_retry_sleep_ms = 1; big.max_retry_sleep_ms = std::numeric_limits<long long>::max();
        CHECK(ParentReadiness::retry_sleep_ms(ParentReadiness::sanitize(big), 200) > 0);  // no overflow to negative
        std::printf("case C5: invalid Config sanitized, no overflow\n");
    }

    ABT_OK(ABT_finalize());
    std::printf("PASS: readiness gate bounded and non-blocking on a single execution stream\n");
    return 0;
}
