// Cache::visit gives access to the stored value without copying it, and the
// cu_fuse_read slice logic copies only the requested range.
#include "cache/cache.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define CHECK(e) do { if(!(e)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #e); std::exit(1); } } while(0)
struct Counted {
    static inline int copies = 0;
    std::vector<std::byte> data;
    Counted() = default;
    explicit Counted(size_t n) : data(n) {}
    Counted(const Counted& o) : data(o.data) { ++copies; }
    Counted(Counted&&) noexcept = default;
    Counted& operator=(const Counted& o) { data = o.data; ++copies; return *this; }
    Counted& operator=(Counted&&) noexcept = default;
};

int main() {
    Cache<std::string, std::vector<std::byte>> c;
    std::vector<std::byte> v(1000);
    for(size_t i = 0; i < v.size(); ++i) v[i] = std::byte{static_cast<unsigned char>(i)};
    c.put_force("f", std::vector<std::byte>(v));
    const std::byte* stored = nullptr;
    CHECK(c.visit("f", [&](const std::vector<std::byte>& b) { stored = b.data(); }));
    const std::byte* again = nullptr;
    CHECK(c.visit("f", [&](const std::vector<std::byte>& b) { again = b.data(); }));
    CHECK(stored != nullptr && stored == again);  // same storage: no per-call copy
    CHECK(!c.visit("missing", [&](const std::vector<std::byte>&) { std::abort(); }));
    char buf[64];
    size_t got = 0;
    for(const long off : {0L, 990L, 1000L, 1500L}) {
        got = 0;
        c.visit("f", [&](const std::vector<std::byte>& b) {
            if(off < static_cast<long>(b.size())) { got = std::min(sizeof buf, b.size() - off); std::memcpy(buf, b.data() + off, got); }
        });
        if(off == 0) { CHECK(got == 64); CHECK(static_cast<unsigned char>(buf[63]) == 63); }
        if(off == 990) { CHECK(got == 10); CHECK(static_cast<unsigned char>(buf[0]) == 222); }
        if(off >= 1000) CHECK(got == 0);
    }
    // Copy counting: visit must not copy the stored value (the old get() copied
    // the whole cached file per FUSE read). Address comparison alone cannot
    // detect a copy because the allocator may reuse the same block.
    Cache<std::string, Counted> cc;
    cc.put_force("big", Counted(1 << 20));
    Counted::copies = 0;
    for(int i = 0; i < 100; ++i) CHECK(cc.visit("big", [](const Counted& v) { CHECK(v.data.size() == (1u << 20)); }));
    std::printf("  copies during 100 visits: %d\n", Counted::copies);
    CHECK(Counted::copies == 0);
    (void)cc.get("big");
    CHECK(Counted::copies == 1);  // control: get() does copy
    std::printf("PASS: Cache::visit no-copy access and slice bounds\n");
    return 0;
}
