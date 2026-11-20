// A stalled log sink must not block logging threads once async mode is on
// (review r2 finding 2). The inner sink blocks until released; a producer that
// logs 10000 lines must finish quickly, overflow is counted, and everything
// queued is written after release. Mutation: with async disabled the producer
// blocks on the stalled sink -> alarm -> rc 2.
#include "fs/async_log_sink.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <vector>
#include <string>

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while(0)

struct StallSink : AixLog::Sink {
    StallSink() : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)) {}
    std::atomic<bool> release{false};
    std::atomic<int> lines{0};
    std::atomic<int> dropped_notes{0};
    void log(const AixLog::Metadata&, const std::string& m) override {
        while(!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));  // stalled Lustre write
        if(m.find("dropped") != std::string::npos) dropped_notes++; else lines++;
    }
};

static void on_alarm(int) { const char m[] = "FAIL: logging thread blocked on a stalled sink\n"; (void)!write(2, m, sizeof m - 1); _exit(2); }

int main() {
    std::signal(SIGALRM, on_alarm);
    alarm(30);
    auto stall = std::make_shared<StallSink>();
    auto sink = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{stall}, 1000);
    AixLog::Log::init({sink});
    sink->enable_async();

    const auto t0 = std::chrono::steady_clock::now();
    for(int i = 0; i < 10000; ++i) LOG(INFO, "TEST") << "line " << i << std::endl;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms < 2000);
    CHECK(stall->lines.load() == 0);  // the sink really is stalled

    stall->release = true;
    CHECK(sink->stop(std::chrono::milliseconds(5000)) == 0);  // clean drain once the sink makes progress
    const int written = stall->lines.load();
    // queue capacity plus whatever batch the writer had already taken before it stalled
    CHECK(written >= 1000 && written < 10000);
    std::printf("written=%d dropped=%llu\n", written, sink->dropped_total());
    CHECK(sink->dropped_total() == static_cast<unsigned long long>(10000 - written));
    CHECK(stall->dropped_notes.load() >= 1);
    std::printf("PASS: 10000 LOG calls returned in %lld ms with sink stalled; %d written, %llu dropped and reported\n",
                (long long)ms, written, sink->dropped_total());
    AixLog::Log::init({});

    // Case 2 (review r3 finding 2): the inner sink blocks forever. stop() must
    // return within its timeout, report the lines it discarded, and later
    // logging must not touch the stalled sink. Concurrent producers keep
    // logging during shutdown.
    {
        auto forever = std::make_shared<StallSink>();  // never released
        auto s2 = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{forever}, 100);
        AixLog::Log::init({s2});
        CHECK(s2->enable_async());
        for(int i = 0; i < 50; ++i) LOG(INFO, "TEST") << "before " << i << std::endl;
        std::atomic<bool> go{true};
        std::vector<std::thread> producers;
        for(int t = 0; t < 4; ++t)
            producers.emplace_back([&go] { while(go.load()) LOG(WARNING, "PROD") << "concurrent" << std::endl; });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const auto t1 = std::chrono::steady_clock::now();
        const unsigned long long unconfirmed = s2->stop(std::chrono::milliseconds(300));
        const auto stop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count();
        CHECK(stop_ms >= 290 && stop_ms < 1000);
        CHECK(s2->abandoned());
        CHECK(unconfirmed >= 1);
        const auto t2 = std::chrono::steady_clock::now();
        for(int i = 0; i < 1000; ++i) LOG(INFO, "TEST") << "after abandon " << i << std::endl;  // must not block
        const auto after_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t2).count();
        go = false;
        for(auto& p : producers) p.join();
        CHECK(after_ms < 1000);
        CHECK(!s2->enable_async());  // refused: the stalled writer still owns the inner sink
        CHECK(s2->stop(std::chrono::milliseconds(10)) == 0);  // idempotent
        const auto snap2 = s2->snapshot();
        CHECK(snap2.unconfirmed_at_stop() == unconfirmed);
        std::printf("case 2: permanent stall -> stop() returned in %lld ms, %llu lines unconfirmed at stop "
                    "(%llu queued discarded, %llu in flight), lost_definite=%llu; 1000 later LOG calls took %lld ms\n",
                    (long long)stop_ms, unconfirmed, snap2.discarded_on_stop, snap2.in_flight_at_stop,
                    snap2.lost_definite(), (long long)after_ms);
        AixLog::Log::init({});
        // s2 is destroyed here while its detached writer is still blocked
        // inside forever->log(); the writer owns the state, so this is safe.
    }
    // Case 3: ordering under a clean drain is FIFO.
    {
        struct Rec : AixLog::Sink {
            Rec() : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)) {}
            std::vector<std::string> got;
            void log(const AixLog::Metadata&, const std::string& m) override { got.push_back(m); }
        };
        auto rec = std::make_shared<Rec>();
        auto s3 = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{rec}, 100000);
        AixLog::Log::init({s3});
        CHECK(s3->enable_async());
        for(int i = 0; i < 5000; ++i) LOG(INFO, "ORD") << i << std::endl;
        CHECK(s3->stop(std::chrono::milliseconds(5000)) == 0);
        AixLog::Log::init({});
        CHECK(rec->got.size() == 5000);
        for(int i = 0; i < 5000; ++i) CHECK(rec->got[i] == std::to_string(i));
        std::printf("case 3: 5000 lines drained in FIFO order\n");
    }
    // Case 4 (review r4 finding 4): exact accounting. 50 accepted lines, writer
    // stalls on the first one it took; stop() must report all 50 as unconfirmed
    // at stop (queued discarded + in flight), and none as overflow drops.
    {
        auto forever = std::make_shared<StallSink>();
        auto s4 = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{forever}, 100);
        AixLog::Log::init({s4});
        CHECK(s4->enable_async());
        for(int i = 0; i < 50; ++i) LOG(INFO, "TEST") << "acc " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));  // writer has taken a batch and is stalled
        const unsigned long long unconfirmed = s4->stop(std::chrono::milliseconds(100));
        std::printf("case 4: unconfirmed=%llu discarded=%llu in_flight_at_stop=%llu dropped_total=%llu\n", unconfirmed,
                    s4->discarded_on_stop(), s4->in_flight_at_stop(), s4->dropped_total());
        CHECK(unconfirmed == 50);
        CHECK(s4->discarded_on_stop() + s4->in_flight_at_stop() == 50);
        CHECK(s4->in_flight_at_stop() >= 1);  // the writer had taken at least the line it is stalled on
        CHECK(s4->dropped_total() == 0);
        AixLog::Log::init({});
        // Case 6 (review r5 finding 2): the sink recovers after the abandon. The
        // in-flight lines complete late and are counted as completed, not lost;
        // discarded queued lines are never written.
        const unsigned long long inflight = s4->in_flight_at_stop();
        const unsigned long long discarded = s4->discarded_on_stop();
        CHECK(s4->in_flight_unresolved() == inflight);
        forever->release = true;
        for(int i = 0; i < 500 && s4->in_flight_unresolved() > 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::printf("case 6: recovered writer: in_flight_at_stop=%llu completed_after_abandon=%llu unresolved=%llu "
                    "written=%d discarded=%llu lost_definite=%llu\n", inflight, s4->completed_after_abandon(),
                    s4->in_flight_unresolved(), forever->lines.load(), discarded, s4->lost_definite());
        CHECK(s4->in_flight_unresolved() == 0);
        CHECK(s4->completed_after_abandon() == inflight);
        CHECK(static_cast<unsigned long long>(forever->lines.load()) == inflight);  // discarded lines never written
        CHECK(s4->lost_definite() == discarded);
    }
    // Case 5 (review r4 finding 1): no producer thread may ever enter the
    // stalled inner sink, including producers racing the abandon transition.
    // The inner sink records calls from any thread other than the writer.
    {
        int bad_total = 0;
        for(int iter = 0; iter < 40; ++iter) {
            struct Spy : AixLog::Sink {
                Spy() : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)) {}
                std::atomic<int> callers{0};
                std::atomic<int> producer_entries{0};
                std::thread::id writer_id{};
                std::atomic<bool> writer_seen{false};
                void log(const AixLog::Metadata&, const std::string&) override {
                    if(!writer_seen.exchange(true)) writer_id = std::this_thread::get_id();
                    else if(std::this_thread::get_id() != writer_id) { producer_entries++; return; }  // would block in real life
                    while(true) std::this_thread::sleep_for(std::chrono::seconds(1));  // writer stalls forever
                }
            };
            auto spy = std::make_shared<Spy>();
            auto s5 = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{spy}, 64);
            AixLog::Log::init({s5});
            CHECK(s5->enable_async());
            LOG(INFO, "T") << "first" << std::endl;
            while(!spy->writer_seen.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::atomic<bool> go{true};
            std::vector<std::thread> producers;
            for(int t = 0; t < 6; ++t)
                producers.emplace_back([&go] { while(go.load()) LOG(INFO, "P") << "x" << std::endl; });
            s5->stop(std::chrono::milliseconds(5));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            go = false;
            for(auto& pr : producers) pr.join();
            bad_total += spy->producer_entries.load();
            AixLog::Log::init({});
        }
        std::printf("case 5: 40 abandon races x 6 producers: producer entries into stalled sink = %d\n", bad_total);
        CHECK(bad_total == 0);
    }
    // Case 7 (review r6 nonblocking finding 2): coherent counter snapshot.
    // 20000 lines are in flight when stop() gives up; the sink then recovers
    // and the detached writer completes them one by one while a reader takes
    // snapshots. Every snapshot must satisfy
    //   in_flight_at_stop == completed_after_abandon + in_flight_unresolved.
    {
        struct SlowRelease : AixLog::Sink {
            SlowRelease() : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)) {}
            std::atomic<int> allowance{0};  // lines the sink may complete; the rest stall
            std::atomic<int> lines{0};
            void log(const AixLog::Metadata&, const std::string&) override {
                while(lines.load() >= allowance.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                lines++;
            }
        };
        auto slow = std::make_shared<SlowRelease>();
        auto s7 = std::make_shared<AsyncLogSink>(std::vector<std::shared_ptr<AixLog::Sink>>{slow}, 100000);
        AixLog::Log::init({s7});
        CHECK(s7->enable_async());
        LOG(INFO, "T") << "first" << std::endl;  // writer takes this one and stalls
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for(int i = 0; i < 20000; ++i) LOG(INFO, "T") << i << std::endl;
        // let the writer finish exactly line 1, take the remaining 20000 as one
        // batch, and stall on the first of them
        slow->allowance = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        s7->stop(std::chrono::milliseconds(50));
        AixLog::Log::init({});
        const auto at_stop = s7->snapshot();
        CHECK(at_stop.abandoned);
        CHECK(at_stop.in_flight_at_stop == 20000);
        CHECK(at_stop.discarded_on_stop == 0);
        std::atomic<bool> reading{true};
        std::atomic<long long> snaps{0}, incoherent{0};
        std::thread reader([&] {
            while(reading.load()) {
                const auto sn = s7->snapshot();
                if(sn.in_flight_at_stop != sn.completed_after_abandon + sn.in_flight_unresolved) incoherent++;
                snaps++;
            }
        });
        slow->allowance = 1 << 30;
        for(int i = 0; i < 5000 && s7->snapshot().in_flight_unresolved > 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        reading = false;
        reader.join();
        const auto end = s7->snapshot();
        std::printf("case 7: in_flight_at_stop=%llu completed_after_abandon=%llu unresolved=%llu; %lld snapshots during "
                    "recovery, %lld incoherent\n", end.in_flight_at_stop, end.completed_after_abandon,
                    end.in_flight_unresolved, snaps.load(), incoherent.load());
        CHECK(end.in_flight_unresolved == 0);
        CHECK(end.completed_after_abandon == end.in_flight_at_stop);
        CHECK(snaps.load() >= 100);
        CHECK(incoherent.load() == 0);
    }
    std::printf("PASS: async log sink\n");
    std::fflush(stdout);
    _exit(0);  // a detached writer is intentionally still blocked in case 2
}
