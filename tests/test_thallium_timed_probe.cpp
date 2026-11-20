// Real Thallium, single handler execution stream (cu_fuse default -es 1):
// a handler ULT runs ParentReadiness::ensure with the production probe shape
// (rpc_is_ready.on(ph).timed(remaining)) against a "parent" provider whose
// rpc_is_ready handler never responds. The waiting handler must give up by its
// deadline (thallium::timeout is caught), and a second, unrelated RPC served on
// the same stream must still complete while the first is waiting.
// (review r2 finding 5: real lookup/timed-forward integration; r3 finding 4:
// the ping is sent only after the child has entered its timed probe)
#include "cache/parent_readiness.h"
#include <thallium.hpp>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace tl = thallium;
#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)
static void on_alarm(int) { const char m[] = "FAIL: hung (timed probe not bounded or stream parked)\n"; (void)!write(2, m, sizeof m - 1); _exit(2); }
static long long now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char** argv) {
    std::signal(SIGALRM, on_alarm);
    alarm(30);
    const char* proto = argc > 1 ? argv[1] : "na+sm";
    // server mode, 1 handler ES (like -es 1)
    tl::engine engine(proto, THALLIUM_SERVER_MODE, true, 1);
    const std::string self = static_cast<std::string>(engine.self());

    std::atomic<bool> stop_parent{false};
    // "parent" never answers rpc_is_ready until told to stop (models a parent that never responds)
    tl::remote_procedure rpc_is_ready = engine.define("rpc_is_ready", [&](const tl::request& req) {
        while(!stop_parent.load()) tl::thread::sleep(engine, 5);
        req.respond(false);
    });
    std::atomic<int> pings{0};
    tl::remote_procedure ping = engine.define("ping", [&](const tl::request& req) { pings++; req.respond(1); });

    long long waited = -1; int result = -1; int probe_errors = 0;
    std::atomic<int> probes_started{0};
    std::atomic<bool> child_done{false};
    // Endpoint resolved once up front, as cu_fuse now does at startup; the
    // handler only copies it (no per-request lookup).
    const tl::endpoint parent_ep = engine.lookup(self);
    // the "child handler" doing the readiness wait
    tl::remote_procedure child = engine.define("child", [&](const tl::request& req) {
        ParentReadiness::Gate gate;
        ParentReadiness::Config cfg; cfg.timeout_ms = 600; cfg.max_probe_timeout_ms = 200;
        cfg.deadline_ms = now_ms() + cfg.timeout_ms;
        tl::provider_handle ph(parent_ep, 0);
        ParentReadiness::Hooks h;
        h.probe = [&](long long timeout_ms) {
            probes_started++;
            try {
                const bool ready = rpc_is_ready.on(ph).timed(std::chrono::milliseconds(timeout_ms));
                return ready ? ParentReadiness::Probe::ready : ParentReadiness::Probe::not_ready;
            } catch(const std::exception&) { probe_errors++; return ParentReadiness::Probe::error; }
        };
        h.sleep_ms = [&](long long ms) { tl::thread::sleep(engine, static_cast<double>(ms)); };
        const long long t0 = now_ms();
        result = ParentReadiness::ensure(gate, cfg, h) ? 1 : 0;
        waited = now_ms() - t0;
        child_done = true;
        req.respond(result);
    });

    auto me = engine.lookup(self);
    auto child_req = child.on(me).async();
    // Handshake (review r3 finding 4): wait until the child handler is inside
    // its timed probe, so the ping below provably runs during the wait.
    while(probes_started.load() == 0) tl::thread::sleep(engine, 1);
    const long long probe_seen_at = now_ms();
    // while the child handler waits, an unrelated RPC must be served on the same single stream
    const long long t_ping = now_ms();
    int pr = ping.on(me)();
    const long long ping_ms = now_ms() - t_ping;
    const bool child_still_waiting_after_ping = !child_done.load();
    int cr = child_req.wait();
    stop_parent = true;
    CHECK(pr == 1 && pings == 1);
    CHECK(cr == 0 && result == 0);
    CHECK(probe_errors >= 1);           // thallium::timeout was raised and caught
    CHECK(waited >= 550 && waited < 600 + 250);
    CHECK(ping_ms < 300);
    CHECK(child_still_waiting_after_ping);  // ping completed while the child was still inside ensure()
    (void)probe_seen_at;
    std::printf("PASS: proto=%s child gave up after %lld ms (budget 600, %d timed-out probes); unrelated RPC served in %lld ms on the same stream\n",
                proto, waited, probe_errors, ping_ms);
    tl::thread::sleep(engine, 50);
    engine.finalize();
    return 0;
}
