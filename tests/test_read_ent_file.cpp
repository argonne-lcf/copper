// Util::read_ent_file must loop over short reads and never return fabricated
// zero-filled tails (review r2 finding 3). read() is interposed so it returns
// at most 7 bytes per call (and EINTR once); a truncation case makes the file
// shrink between fstat and read.
#include "fs/util.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <new>
#include <stdexcept>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/wait.h>

static int count_fds() {
    int n = 0;
    DIR* d = opendir("/proc/self/fd");
    while(readdir(d) != nullptr) ++n;
    closedir(d);
    return n;
}

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

static int g_mode = 0;           // 0 passthrough, 1 short reads, 2 truncate-after-fstat
static bool g_eintr_once = false;
static std::string g_trunc_path;
static int g_calls = 0;

extern "C" ssize_t read(int fd, void* buf, size_t n) {
    using fn = ssize_t (*)(int, void*, size_t);
    static fn real = reinterpret_cast<fn>(dlsym(RTLD_NEXT, "read"));
    if(g_mode == 0) return real(fd, buf, n);
    ++g_calls;
    if(g_eintr_once) { g_eintr_once = false; errno = EINTR; return -1; }
    if(g_mode == 2 && g_calls == 1) { CHECK(truncate(g_trunc_path.c_str(), 10) == 0); }
    return real(fd, buf, n < 7 ? n : 7);
}

static std::string make_file(const char* name, size_t size) {
    std::string path = std::string("/tmp/copper_read_test_") + std::to_string(getpid()) + "_" + name;
    FILE* f = std::fopen(path.c_str(), "wb");
    for(size_t i = 0; i < size; ++i) std::fputc('A' + static_cast<int>(i % 26), f);
    std::fclose(f);
    return path;
}

int main() {
    const std::string p1 = make_file("short", 1000);
    g_mode = 1; g_eintr_once = true; g_calls = 0;
    auto bytes = Util::read_ent_file(p1);
    g_mode = 0;
    CHECK(bytes.size() == 1000);
    for(size_t i = 0; i < bytes.size(); ++i) CHECK(static_cast<char>(bytes[i]) == 'A' + static_cast<int>(i % 26));
    CHECK(g_calls > 100);  // really exercised many short reads
    std::printf("short reads: 1000 bytes intact over %d read() calls (incl. one EINTR)\n", g_calls);

    const std::string p2 = make_file("trunc", 1000);
    g_trunc_path = p2; g_mode = 2; g_calls = 0;
    auto t = Util::read_ent_file(p2);
    g_mode = 0;
    CHECK(t.size() == 10);  // only verified bytes, no zero-filled tail
    for(size_t i = 0; i < t.size(); ++i) CHECK(static_cast<char>(t[i]) == 'A' + static_cast<int>(i));
    std::printf("truncated during read: returned %zu verified bytes (fstat said 1000)\n", t.size());
    unlink(p1.c_str()); unlink(p2.c_str());

    // Review r7 finding 3: the descriptor is released when allocating the
    // result throws. A 64 GiB sparse file under a 4 GiB address-space limit
    // makes the vector allocation throw after open+fstat succeeded. Runs in a
    // forked child so the limit does not affect the rest of the test.
    const std::string p3 = make_file("huge", 16);
    CHECK(truncate(p3.c_str(), 64LL << 30) == 0);
    const pid_t child = fork();
    CHECK(child >= 0);
    if(child == 0) {
        struct rlimit lim{4ULL << 30, 4ULL << 30};
        if(setrlimit(RLIMIT_AS, &lim) != 0) _exit(3);
        const int before = count_fds();
        int thrown = 0;
        for(int i = 0; i < 20; ++i) {
            try {
                (void)Util::read_ent_file(p3);
            } catch(const std::bad_alloc&) {
                ++thrown;
            } catch(const std::length_error&) {
                ++thrown;
            }
        }
        const int after = count_fds();
        std::printf("allocation failure: %d/20 threw, open fds %d -> %d\n", thrown, before, after);
        std::fflush(stdout);
        _exit(thrown == 20 && after == before ? 0 : 1);
    }
    int st = 0;
    CHECK(waitpid(child, &st, 0) == child);
    unlink(p3.c_str());
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    std::printf("PASS: read_ent_file handles short reads, EINTR and concurrent truncation, and releases its fd on allocation failure\n");
    return 0;
}
