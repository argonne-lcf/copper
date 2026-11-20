#ifndef PARENT_READINESS_H
#define PARENT_READINESS_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <mutex>

#include "abt_sync.h"

// Bounded, once-per-process wait for the parent provider to report ready.
//
// Callers are RPC handler ULTs, so:
//  - the gate lock is an AbtMutex (a handler blocked on it yields instead of
//    parking the shared execution stream), and
//  - every probe and back-off sleep is supplied by the caller as an
//    Argobots-aware hook (timed RPC, tl::thread::sleep).
//
// Endpoint resolution is NOT done here: cu_fuse resolves the parent endpoint
// once during engine startup (under the startup watchdog) and handlers reuse it.
//
// Bounding: each caller has one absolute deadline (Config::deadline_ms, or
// entry time + timeout_ms). It covers (1) acquiring the gate (cooperative
// trylock + ULT sleep, never an untimed lock), (2) every probe (timed to the
// remaining budget) and back-off sleep, and (3) success: a ready answer that
// arrives after the deadline still marks the gate ready (it is true) but this
// caller returns false. Callers queued behind a holder whose wait failed fail
// immediately (shared failure) instead of each starting a new full wait.
// Gate::reset() must only be called while no ensure() can run (engine start).
namespace ParentReadiness {

enum class Probe { ready, not_ready, error };

struct Config {
    long long timeout_ms = 120000;           // per-caller absolute budget
    long long max_probe_timeout_ms = 5000;   // cap on one probe RPC
    long long initial_retry_sleep_ms = 10;
    long long max_retry_sleep_ms = 1000;
    long long gate_poll_ms = 1;              // cooperative poll interval while the gate is held
    // Absolute deadline in now_ms() units. If >= 0 it overrides timeout_ms, so
    // a caller can start its budget before endpoint resolution.
    long long deadline_ms = -1;
};

struct Hooks {
    std::function<Probe(long long probe_timeout_ms)> probe;
    std::function<void(long long ms)> sleep_ms;
    std::function<long long()> now_ms;  // optional; defaults to steady_clock
    std::function<void(int attempts, long long waited_ms, long long next_sleep_ms)> on_retry;
    std::function<void(int attempts, long long waited_ms)> on_ready;
    std::function<void(int attempts, long long waited_ms)> on_timeout;
    std::function<void()> on_shared_failure;
};

class Gate {
    public:
    Gate() = default;
    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;
    bool ready() const { return ready_.load(std::memory_order_acquire); }
    void reset() {
        ready_.store(false, std::memory_order_release);
        failures_.store(0, std::memory_order_release);
    }

    private:
    std::atomic<bool> ready_{false};
    std::atomic<unsigned long long> failures_{0};
    AbtMutex mutex_;
    friend bool ensure(Gate&, const Config&, const Hooks&);
};

inline long long retry_sleep_ms(const Config& c, const int attempts) {
    long long s = c.initial_retry_sleep_ms;
    for(int i = 1; i < attempts && s < c.max_retry_sleep_ms; ++i) s = (s > c.max_retry_sleep_ms / 2) ? c.max_retry_sleep_ms : s * 2;
    return std::min(s, c.max_retry_sleep_ms);
}

// Saturating helpers: Config is public, so tolerate hostile/invalid values
// instead of overflowing signed arithmetic.
inline long long sat_add(const long long a, const long long b) {
    constexpr long long hi = std::numeric_limits<long long>::max();
    constexpr long long lo = std::numeric_limits<long long>::min();
    if(b > 0 && a > hi - b) return hi;
    if(b < 0 && a < lo - b) return lo;
    return a + b;
}

inline Config sanitize(Config c) {
    c.timeout_ms = std::max(0LL, c.timeout_ms);
    c.max_probe_timeout_ms = std::max(1LL, c.max_probe_timeout_ms);
    c.initial_retry_sleep_ms = std::max(1LL, c.initial_retry_sleep_ms);
    c.max_retry_sleep_ms = std::max(c.initial_retry_sleep_ms, c.max_retry_sleep_ms);
    c.gate_poll_ms = std::max(1LL, c.gate_poll_ms);
    return c;
}

inline bool ensure(Gate& gate, const Config& raw_config, const Hooks& hooks) {
    const Config config = sanitize(raw_config);
    const auto now = [&]() -> long long {
        if(hooks.now_ms) return hooks.now_ms();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const long long start = now();
    const long long deadline = config.deadline_ms >= 0 ? config.deadline_ms : sat_add(start, config.timeout_ms);
    // The deadline is checked before the ready fast path, so a caller whose
    // budget was already spent (e.g. by earlier work) fails even when the
    // parent is known to be ready.
    if(start > deadline) {
        if(hooks.on_timeout) hooks.on_timeout(0, 0);
        return false;
    }
    if(gate.ready()) return true;
    const unsigned long long failures_seen = gate.failures_.load(std::memory_order_acquire);

    // Deadline-aware cooperative acquisition.
    while(!gate.mutex_.try_lock()) {
        if(gate.ready()) return now() <= deadline;
        const long long left = deadline - now();
        if(left <= 0) {
            if(hooks.on_timeout) hooks.on_timeout(0, now() - start);
            return false;
        }
        hooks.sleep_ms(std::min(config.gate_poll_ms, left));
    }
    std::lock_guard<AbtMutex> guard(gate.mutex_, std::adopt_lock);
    if(gate.ready()) return now() <= deadline;
    if(gate.failures_.load(std::memory_order_acquire) != failures_seen) {
        // A wait that overlapped ours already exhausted its budget.
        if(hooks.on_shared_failure) hooks.on_shared_failure();
        return false;
    }

    int attempts = 0;
    while(true) {
        const long long remaining = deadline - now();
        if(remaining <= 0) {
            gate.failures_.fetch_add(1, std::memory_order_acq_rel);
            if(hooks.on_timeout) hooks.on_timeout(attempts, now() - start);
            return false;
        }
        ++attempts;
        if(hooks.probe(std::min(remaining, config.max_probe_timeout_ms)) == Probe::ready) {
            gate.ready_.store(true, std::memory_order_release);
            const long long done = now();
            if(done > deadline) {  // late success: parent is ready, but this caller's budget is spent
                if(hooks.on_timeout) hooks.on_timeout(attempts, done - start);
                return false;
            }
            if(hooks.on_ready) hooks.on_ready(attempts, done - start);
            return true;
        }
        const long long left = deadline - now();
        if(left <= 0) continue;  // report timeout at loop top
        const long long sleep = std::min(retry_sleep_ms(config, attempts), left);
        if(hooks.on_retry) hooks.on_retry(attempts, now() - start, sleep);
        hooks.sleep_ms(sleep);
    }
}

}  // namespace ParentReadiness

#endif  // PARENT_READINESS_H
