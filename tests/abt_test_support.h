#ifndef ABT_TEST_SUPPORT_H
#define ABT_TEST_SUPPORT_H
#include <abt.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#define ABT_OK(expr) do { int abt_rc_ = (expr); if(abt_rc_ != ABT_SUCCESS) { \
    std::fprintf(stderr, "%s:%d: %s failed rc=%d\n", __FILE__, __LINE__, #expr, abt_rc_); std::exit(1); } } while(0)
#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

static void on_alarm(int) {
    const char msg[] = "FAIL: deadlock - a blocked handler parked the only execution stream\n";
    (void)!write(2, msg, sizeof(msg) - 1);
    _exit(2);
}

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Cooperative sleep: yields the ULT until `ms` has elapsed (what tl::thread::sleep does).
static void abt_sleep_ms(long long ms) {
    const long long end = now_ms() + ms;
    while(now_ms() < end) ABT_OK(ABT_thread_yield());
}

static ABT_pool single_stream_pool() {
    ABT_xstream xs; ABT_pool pool;
    ABT_OK(ABT_xstream_self(&xs));
    ABT_OK(ABT_xstream_get_main_pools(xs, 1, &pool));
    return pool;
}
#endif
