// Response serialization cost for std::vector<std::byte> (torch-import slowdown).
//
// Part 1, wire compatibility: with cereal::BinaryOutputArchive, the new bulk
// std::vector<std::byte> output must be byte-identical to the legacy
// per-element enum path (reproduced with an enum class : unsigned char, which
// still takes cereal's generic per-element path) and to std::vector<uint8_t>.
// Round-trip must restore the data.
//
// Part 2, real Thallium on na+sm with a single handler execution stream: an
// RPC returns std::pair<ssize_t, std::vector<std::byte>> of SIZE_MB (default
// 64). Checks the content, and that the respond() of the large payload does
// not hold the handler stream for long (a tiny ping RPC issued concurrently
// must complete quickly). Without byte_vector_serialization.h the respond is
// per-byte and takes seconds; with it, milliseconds.
//
// COPPER_TEST_LEGACY_BYTE_SERIALIZATION=1 at compile time omits the header
// (mutation check).
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#ifndef COPPER_TEST_LEGACY_BYTE_SERIALIZATION
#include "copper/byte_vector_serialization.h"
#endif
#include <cereal/archives/binary.hpp>

#include <atomic>
#include <csignal>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <vector>

namespace tl = thallium;

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

static void on_alarm(int) { const char m[] = "FAIL: timed out (respond held the handler stream)\n"; (void)!write(2, m, sizeof m - 1); _exit(2); }

enum class LegacyByte : unsigned char {};

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template <class T> static std::string ser(const T& v) {
    std::ostringstream os(std::ios::binary);
    {
        cereal::BinaryOutputArchive ar(os);
        ar(v);
    }
    return os.str();
}

int main() {
    // ---- part 1: wire compatibility
    for(size_t n : {size_t(0), size_t(1), size_t(7), size_t(4096), size_t(1000003)}) {
        std::vector<std::byte> b(n);
        std::vector<LegacyByte> l(n);
        std::vector<unsigned char> u(n);
        for(size_t i = 0; i < n; ++i) {
            const unsigned char c = static_cast<unsigned char>((i * 131u + 7u) & 0xff);
            b[i] = std::byte{c};
            l[i] = static_cast<LegacyByte>(c);
            u[i] = c;
        }
        const std::string sb = ser(b), sl = ser(l), su = ser(u);
        CHECK(sb == sl);
        CHECK(sb == su);
        std::istringstream is(sb, std::ios::binary);
        std::vector<std::byte> back;
        {
            cereal::BinaryInputArchive ar(is);
            ar(back);
        }
        CHECK(back == b);
    }
    std::printf("part 1: std::vector<std::byte> wire bytes identical to legacy per-element encoding; round-trip ok\n");

    // ---- part 2: real Thallium, one engine, one handler stream (cu_fuse -es 1 shape)
    const char* env = std::getenv("SIZE_MB");
    const size_t size_mb = env ? std::strtoul(env, nullptr, 10) : 64;
    const size_t nbytes = size_mb << 20;
    std::signal(SIGALRM, on_alarm);
    alarm(100);

    tl::engine engine("na+sm", THALLIUM_SERVER_MODE, true, 1);
    const std::string self = static_cast<std::string>(engine.self());
    std::vector<std::byte> payload(nbytes);
    for(size_t i = 0; i < nbytes; ++i) payload[i] = std::byte{static_cast<unsigned char>((i * 2654435761u) >> 24)};
    std::atomic<long long> respond_ms{-1};
    std::atomic<int> big_entered{0};
    tl::remote_procedure big = engine.define("big", [&](const tl::request& req) {
        big_entered = 1;
        std::pair<ssize_t, std::vector<std::byte>> resp(static_cast<ssize_t>(payload.size()), payload);
        const long long t0 = now_ms();
        req.respond(resp);
        respond_ms = now_ms() - t0;
    });
    tl::remote_procedure ping = engine.define("ping", [](const tl::request& req) { req.respond(1); });
    tl::endpoint me = engine.lookup(self);

    const long long t0 = now_ms();
    auto big_req = big.on(me).async();
    while(big_entered.load() == 0) tl::thread::sleep(engine, 1);
    const long long tp = now_ms();
    int one = ping.on(me)();
    const long long ping_ms = now_ms() - tp;
    std::pair<ssize_t, std::vector<std::byte>> got = big_req.wait();
    const long long total_ms = now_ms() - t0;

    CHECK(one == 1);
    CHECK(got.first == static_cast<ssize_t>(nbytes));
    CHECK(got.second == payload);
    std::printf("part 2: %zu MB response: server respond()=%lld ms, client total=%lld ms, concurrent ping=%lld ms\n", size_mb,
                respond_ms.load(), total_ms, ping_ms);
    std::fflush(stdout);
    const long long limit_ms = static_cast<long long>(size_mb) * 5 + 200;  // bulk ~0.5 ms/MB; per-byte ~11-18 ms/MB
    CHECK(respond_ms.load() < limit_ms);
    CHECK(ping_ms < limit_ms);
    std::printf("PASS: bulk std::vector<std::byte> serialization (limit %lld ms)\n", limit_ms);
    engine.finalize();
    return 0;
}
