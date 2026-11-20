// Review r4 finding 2: the parent endpoint must not be freed while a handler
// holds it. LeasedPtr::close() blocks new leases, waits for live ones, frees
// only when none remain, and leaks on timeout instead of freeing.
#include "copper/leased_ptr.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>
#define CHECK(e) do { if(!(e)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #e); std::exit(1); } } while(0)

static std::atomic<int> live_objects{0};
struct Tracked {
    int magic = 0x5eed;
    Tracked() { live_objects++; }
    ~Tracked() { magic = 0; live_objects--; }
};

int main() {
    // A: close waits for a live lease, then frees
    {
        LeasedPtr<Tracked> lp;
        lp.reset(std::make_unique<Tracked>());
        std::atomic<bool> holding{false}, release{false}, saw_bad{false};
        std::thread user([&] {
            LeasedPtr<Tracked>::Lease lease(lp);
            holding = true;
            while(!release) { if(lease.get().magic != 0x5eed) saw_bad = true; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
            if(lease.get().magic != 0x5eed) saw_bad = true;
        });
        while(!holding) std::this_thread::yield();
        std::thread closer([&] { CHECK(lp.close(std::chrono::milliseconds(5000))); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CHECK(live_objects.load() == 1);  // not freed while leased
        bool threw = false;
        try { LeasedPtr<Tracked>::Lease late(lp); } catch(const std::runtime_error&) { threw = true; }
        CHECK(threw);  // new leases refused once closing
        release = true;
        user.join();
        closer.join();
        CHECK(!saw_bad);
        CHECK(live_objects.load() == 0);
        std::printf("case A: close waited for live lease, refused new lease, freed after release\n");
    }
    // B: timeout leaks rather than frees
    {
        LeasedPtr<Tracked> lp;
        lp.reset(std::make_unique<Tracked>());
        const int before = live_objects.load();
        auto* lease = new LeasedPtr<Tracked>::Lease(lp);
        const auto t0 = std::chrono::steady_clock::now();
        CHECK(!lp.close(std::chrono::milliseconds(100)));
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        CHECK(ms >= 95 && ms < 1000);
        CHECK(live_objects.load() == before);  // leaked, still valid
        CHECK(lease->get().magic == 0x5eed);
        delete lease;
        std::printf("case B: close timed out in %lld ms; object leaked, lease still valid\n", (long long)ms);
    }
    // C: stress — many short leases racing close; no lease ever sees a freed object
    {
        int bad = 0, refused = 0, ok = 0;
        for(int iter = 0; iter < 200; ++iter) {
            LeasedPtr<Tracked> lp;
            lp.reset(std::make_unique<Tracked>());
            std::atomic<int> b{0}, r{0}, k{0};
            std::vector<std::thread> users;
            for(int t = 0; t < 4; ++t)
                users.emplace_back([&] {
                    for(int i = 0; i < 200; ++i) {
                        try { LeasedPtr<Tracked>::Lease l(lp); if(l.get().magic != 0x5eed) b++; k++; }
                        catch(const std::runtime_error&) { r++; }
                    }
                });
            CHECK(lp.close(std::chrono::milliseconds(5000)));
            for(auto& u : users) u.join();
            bad += b; refused += r; ok += k;
        }
        CHECK(bad == 0);
        CHECK(live_objects.load() == 1);  // only case B's leaked object remains
        std::printf("case C: 200 close races x 4 users: ok=%d refused=%d bad=%d\n", ok, refused, bad);
    }
    std::printf("PASS: LeasedPtr\n");
    return 0;
}
