// AixLog per-thread record state (review r3 finding 3).
// Upstream AixLog kept one global severity/tag/function and conditional flag,
// so a record being assembled on thread A could be delivered with thread B's
// severity/tag, or suppressed by B's Conditional. Deterministic interleaving:
//   A: LOG(ERROR, "A") << "ERROR_FROM_A"   (no newline yet -> buffered)
//   B: LOG(INFO, "B")  << "INFO_FROM_B" << endl
//   B: LOG(INFO, "B")  << COND(false) << "SUPPRESSED_B" << endl
//   A: << endl                              (flush)
// Expected: ERROR_FROM_A delivered as severity=error tag=A; INFO_FROM_B as
// info/B; SUPPRESSED_B not delivered; A's record not suppressed by B's COND.
// Plus a stress run: 4 threads x 20000 records with distinct severity/tag,
// each record must arrive with its own thread's metadata.
#include "aixlog.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

struct Rec {
    AixLog::Severity sev;
    std::string tag;
    std::string msg;
};

struct Capture : AixLog::Sink {
    Capture() : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)) {}
    std::mutex m;
    std::vector<Rec> recs;
    void log(const AixLog::Metadata& md, const std::string& msg) override {
        std::lock_guard<std::mutex> g(m);
        recs.push_back({md.severity, md.tag.text, msg});
    }
};

int main() {
    auto cap = std::make_shared<Capture>();
    AixLog::Log::init({cap});

    std::mutex m;
    std::condition_variable cv;
    int step = 0;
    auto wait_step = [&](int s) { std::unique_lock<std::mutex> l(m); cv.wait(l, [&] { return step >= s; }); };
    auto set_step = [&](int s) { { std::lock_guard<std::mutex> l(m); step = s; } cv.notify_all(); };

    std::thread a([&] {
        LOG(ERROR, "A") << "ERROR_FROM_A";
        set_step(1);
        wait_step(2);
        LOG(ERROR, "A") << std::endl;  // same severity -> no reset; flushes the buffered text
    });
    std::thread b([&] {
        wait_step(1);
        LOG(INFO, "B") << "INFO_FROM_B" << std::endl;
        LOG(INFO, "B") << COND(false) << "SUPPRESSED_B" << std::endl;
        set_step(2);
    });
    a.join();
    b.join();

    bool sawA = false, sawB = false;
    for(const auto& r : cap->recs) {
        std::printf("  delivered: severity=%s tag=%s msg=%s\n", AixLog::to_string(r.sev).c_str(), r.tag.c_str(), r.msg.c_str());
        CHECK(r.msg.find("SUPPRESSED_B") == std::string::npos);
        if(r.msg.find("ERROR_FROM_A") != std::string::npos) {
            CHECK(r.sev == AixLog::Severity::error);
            CHECK(r.tag == "A");
            sawA = true;
        }
        if(r.msg.find("INFO_FROM_B") != std::string::npos) {
            CHECK(r.sev == AixLog::Severity::info);
            CHECK(r.tag == "B");
            sawB = true;
        }
    }
    CHECK(sawA && sawB);

    // Same-thread leak: COND(false) or a tag must not carry into the next
    // same-severity LOG(...) statement.
    cap->recs.clear();
    LOG(INFO, "X") << COND(false) << "HIDDEN" << std::endl;
    LOG(INFO, "Y") << "VISIBLE_AFTER_COND" << std::endl;
    LOG(INFO) << "UNTAGGED_AFTER_TAG" << std::endl;
    CHECK(cap->recs.size() == 2);
    CHECK(cap->recs[0].msg == "VISIBLE_AFTER_COND" && cap->recs[0].tag == "Y");
    CHECK(cap->recs[1].msg == "UNTAGGED_AFTER_TAG" && cap->recs[1].tag.empty());
    std::printf("  same-thread: COND/tag did not leak into the next statement\n");

    // stress
    cap->recs.clear();
    const AixLog::Severity sevs[4] = {AixLog::Severity::debug, AixLog::Severity::info, AixLog::Severity::warning,
                                      AixLog::Severity::error};
    const char* tags[4] = {"T0", "T1", "T2", "T3"};
    const int N = 20000;
    std::vector<std::thread> ts;
    for(int t = 0; t < 4; ++t)
        ts.emplace_back([&, t] {
            for(int i = 0; i < N; ++i) LOG(sevs[t], tags[t]) << "thread" << t << " " << i << std::endl;
        });
    for(auto& t : ts) t.join();
    int bad = 0;
    std::printf("  stress: delivered=%zu expected=%d clog_state=%d\n", cap->recs.size(), 4 * N, (int)std::clog.rdstate());
    for(const auto& r : cap->recs) {
        const int t = r.msg[6] - '0';
        if(t < 0 || t > 3 || r.sev != sevs[t] || r.tag != tags[t]) ++bad;
    }
    std::map<std::string,int> samples; int shown=0;
    for(const auto& r : cap->recs) { const int t = r.msg[6]-'0'; if((t<0||t>3||r.sev!=sevs[t]||r.tag!=tags[t]) && shown++<5) std::printf("  bad: sev=%s tag=%s msg=[%s]\n", AixLog::to_string(r.sev).c_str(), r.tag.c_str(), r.msg.c_str()); }
    std::printf("  stress: bad=%d\n", bad);
    { std::map<std::string,int> per; for(const auto& r: cap->recs) per[r.tag]++; for(auto& kv: per) std::printf("  tag %s: %d\n", kv.first.c_str(), kv.second); }
    CHECK(cap->recs.size() == static_cast<size_t>(4 * N));
    CHECK(bad == 0);
    AixLog::Log::init({});
    std::printf("PASS: per-thread AixLog metadata; deterministic interleave correct; %d stress records, 0 mislabelled\n", 4 * N);
    return 0;
}
