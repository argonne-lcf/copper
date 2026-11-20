#ifndef ASYNC_LOG_SINK_H
#define ASYNC_LOG_SINK_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../aixlog.h"

// AixLog sink that keeps sink I/O away from the threads that log.
//
// Why: AixLog calls Sink::log() while holding its global recursive std::mutex.
// With a synchronous file/stdout sink, a stalled write (Lustre, a blocked pipe)
// holds that OS mutex. That blocks the OS thread of every RPC handler ULT that
// logs, which is the whole handler execution stream. Once async mode is on,
// log() only does a short bounded enqueue, and a dedicated OS writer thread
// performs the real (possibly blocking) writes.
//
// Overflow: once `capacity` lines are queued, new lines are dropped and
// counted; the writer emits "dropped N log lines" when it next runs. One batch
// the writer has already taken may be in flight beyond `capacity`, so at most
// 2*capacity lines are held in memory.
//
// Shutdown guarantee (stop(timeout)):
//  * If the writer makes progress, every line accepted before stop() is
//    written, in FIFO order, and stop() joins the writer.
//  * If the writer does not finish within `timeout` (an inner write is blocked
//    indefinitely), stop() returns anyway. The writer thread is detached; it
//    shares ownership of the writer state through a shared_ptr, never `this`,
//    so it can safely finish or stay blocked after the sink is destroyed. The
//    lines still queued are discarded (discarded_on_stop(): never written).
//    Lines the writer had already taken are in_flight_at_stop(): their fate is
//    unknown at stop time. If the inner sink later recovers, the detached
//    writer finishes them and completed_after_abandon() counts them, so
//    in_flight_at_stop == completed_after_abandon + in_flight_unresolved in
//    every snapshot() (one lock-protected read of all counters). Separate
//    getter calls are individually correct but are not one atomic snapshot
//    while the detached writer is still completing lines; use snapshot() to
//    reconcile counters. (With several inner sinks a line may have reached
//    some sinks and not others; the in-flight count is per line, not per sink.)
//    stop() returns discarded + in-flight, i.e. lines NOT CONFIRMED written.
//    After an abandoned stop, log() drops and counts lines (lost_after_abandon)
//    instead of writing synchronously into the stalled sink.
//  It is impossible to guarantee both "drain everything" and "terminate on
//  time" against I/O that is blocked forever; bounded termination wins.
//
// Lifecycle: enable_async(), stop() and the destructor are serialized by
// lifecycle_mtx_. After an abandoned stop, enable_async() is refused, because
// the stalled writer still owns the inner sinks.
//
// Fork: cu_fuse daemonizes inside fuse_main() (no -f), and threads do not
// survive fork(). So the sink stays synchronous (the original behaviour, main
// thread only) until enable_async() is called from cu_fuse_init. That runs in
// the daemon, after the fork and before the Thallium engine starts.
class AsyncLogSink final : public AixLog::Sink {
    public:
    using Entry = std::pair<AixLog::Metadata, std::string>;

    AsyncLogSink(std::vector<std::shared_ptr<AixLog::Sink>> inner, std::size_t capacity)
    : AixLog::Sink(AixLog::Filter(AixLog::Severity::trace)), state_(std::make_shared<State>()) {
        state_->inner = std::move(inner);
        state_->capacity = capacity;
    }

    ~AsyncLogSink() override { stop(std::chrono::milliseconds(2000)); }

    void log(const AixLog::Metadata& metadata, const std::string& message) override {
        State& st = *state_;
        // One mode word (review r4 finding 1). Transitions sync->async happen
        // before any handler runs; async->abandoned and async->sync happen under
        // st.mtx, so a producer that saw `async` re-checks under the lock and
        // can never fall through to a synchronous write into a stalled sink.
        const int mode = st.mode.load(std::memory_order_acquire);
        if(mode == State::kAbandoned) {
            st.lost_after_abandon.fetch_add(1, std::memory_order_relaxed);  // never touch the stalled inner sink again
            return;
        }
        if(mode == State::kSync) {
            st.write_inner(metadata, message);
            return;
        }
        {
            std::lock_guard<std::mutex> guard(st.mtx);
            const int m = st.mode.load(std::memory_order_relaxed);
            if(m == State::kAbandoned) {
                st.lost_after_abandon.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if(m == State::kSync) {
                // clean stop completed meanwhile; writer joined and inner sinks are free.
                // Write outside st.mtx is not needed: the writer is gone.
                st.write_inner(metadata, message);
                return;
            }
            if(st.stop || st.queue.size() >= st.capacity) {
                ++st.dropped;
                return;
            }
            st.queue.emplace_back(metadata, message);
        }
        st.cv.notify_one();
    }

    // Idempotent; call once in the final (post-fork) process. Returns false if
    // refused (after an abandoned stop).
    bool enable_async() {
        std::lock_guard<std::mutex> life(lifecycle_mtx_);
        if(state_->mode.load() == State::kAbandoned) return false;
        if(writer_.joinable()) return true;
        {
            std::lock_guard<std::mutex> guard(state_->mtx);
            state_->stop = false;
            state_->finished = false;
        }
        std::shared_ptr<State> st = state_;
        writer_ = std::thread([st] { st->run(); });
        state_->mode.store(State::kAsync, std::memory_order_release);
        return true;
    }

    // Bounded shutdown; see the class comment. Returns the number of accepted
    // lines NOT CONFIRMED written at stop: queued lines discarded (definitely
    // never written) plus lines in flight (outcome unknown, may still complete
    // late). 0 on a clean drain. Use snapshot() for the breakdown.
    unsigned long long stop(std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> life(lifecycle_mtx_);
        State& st = *state_;
        if(!writer_.joinable()) return 0;
        bool finished;
        {
            std::unique_lock<std::mutex> lock(st.mtx);
            st.stop = true;
            st.cv.notify_all();
            finished = st.done_cv.wait_for(lock, timeout, [&st] { return st.finished; });
            if(!finished) {
                // r4 finding 4 / r5 finding 2: queued lines are discarded here
                // and are definitely never written; lines the writer already
                // took (in_flight) are unknown and may still complete late.
                // Overflow drops are accounted separately.
                const unsigned long long discarded = st.queue.size();
                const unsigned long long unconfirmed = discarded + st.in_flight;
                st.queue.clear();
                st.discarded_on_stop.fetch_add(discarded, std::memory_order_relaxed);
                st.in_flight_at_stop.fetch_add(st.in_flight, std::memory_order_relaxed);
                st.dropped_total.fetch_add(st.dropped, std::memory_order_relaxed);  // overflow stays overflow
                st.dropped = 0;
                st.mode.store(State::kAbandoned, std::memory_order_release);
                lock.unlock();
                writer_.detach();  // owns `st` via shared_ptr; safe to outlive the sink
                return unconfirmed;
            }
        }
        writer_.join();
        {
            std::lock_guard<std::mutex> guard(st.mtx);
            st.mode.store(State::kSync, std::memory_order_release);
        }
        return 0;
    }

    // Overflow drops (queue full), reported when the writer runs or at abandon.
    unsigned long long dropped_total() const { return state_->dropped_total.load(); }
    // Queued lines discarded by an abandoned stop(): never written.
    unsigned long long discarded_on_stop() const { return state_->discarded_on_stop.load(); }
    // Lines the writer had taken but not finished when stop() gave up: fate
    // unknown at stop time (snapshot, not a loss count).
    unsigned long long in_flight_at_stop() const { return state_->in_flight_at_stop.load(); }
    // Of in_flight_at_stop(), lines the detached writer completed afterwards.
    unsigned long long completed_after_abandon() const { return state_->completed_after_abandon.load(); }
    // Of in_flight_at_stop(), lines still neither completed nor known lost.
    unsigned long long in_flight_unresolved() const {
        std::lock_guard<std::mutex> guard(state_->mtx);
        return abandoned() ? state_->in_flight : 0;
    }
    // Lines logged after the abandon, discarded without touching the sink.
    unsigned long long lost_after_abandon() const { return state_->lost_after_abandon.load(); }
    // Lines definitely never written because of shutdown.
    unsigned long long lost_definite() const { return discarded_on_stop() + lost_after_abandon(); }
    bool abandoned() const { return state_->mode.load() == State::kAbandoned; }

    // All counters read under one lock (review r6 nonblocking finding 2).
    // Every counter that the invariant relates (in_flight_at_stop,
    // completed_after_abandon, in-flight, discarded_on_stop, dropped_total) is
    // only modified under the same lock, so within one Snapshot:
    //   in_flight_at_stop == completed_after_abandon + in_flight_unresolved
    // lost_after_abandon is incremented lock-free by producers after the
    // abandon; it is monotonic and not part of that invariant.
    struct Snapshot {
        bool abandoned = false;
        unsigned long long dropped_total = 0;
        unsigned long long discarded_on_stop = 0;
        unsigned long long in_flight_at_stop = 0;
        unsigned long long completed_after_abandon = 0;
        unsigned long long in_flight_unresolved = 0;
        unsigned long long lost_after_abandon = 0;
        unsigned long long unconfirmed_at_stop() const { return discarded_on_stop + in_flight_at_stop; }
        unsigned long long lost_definite() const { return discarded_on_stop + lost_after_abandon; }
    };
    Snapshot snapshot() const {
        const State& st = *state_;
        std::lock_guard<std::mutex> guard(st.mtx);
        Snapshot s;
        s.abandoned = st.mode.load() == State::kAbandoned;
        s.dropped_total = st.dropped_total.load();
        s.discarded_on_stop = st.discarded_on_stop.load();
        s.in_flight_at_stop = st.in_flight_at_stop.load();
        s.completed_after_abandon = st.completed_after_abandon.load();
        s.in_flight_unresolved = s.abandoned ? st.in_flight : 0;
        s.lost_after_abandon = st.lost_after_abandon.load();
        return s;
    }

    private:
    struct State {
        std::vector<std::shared_ptr<AixLog::Sink>> inner;
        std::size_t capacity = 0;
        mutable std::mutex mtx;
        std::condition_variable cv;       // producers/stop -> writer
        std::condition_variable done_cv;  // writer -> stop
        std::deque<Entry> queue;
        unsigned long long dropped = 0;  // pending, not yet reported
        bool stop = false;
        bool finished = false;
        unsigned long long in_flight = 0;  // lines the writer took but has not finished writing (under mtx)
        std::atomic<unsigned long long> dropped_total{0};
        std::atomic<unsigned long long> discarded_on_stop{0};
        std::atomic<unsigned long long> in_flight_at_stop{0};
        std::atomic<unsigned long long> completed_after_abandon{0};
        std::atomic<unsigned long long> lost_after_abandon{0};
        static constexpr int kSync = 0, kAsync = 1, kAbandoned = 2;
        std::atomic<int> mode{kSync};

        void write_inner(const AixLog::Metadata& metadata, const std::string& message) {
            for(const auto& sink : inner)
                if(sink->filter.match(metadata)) sink->log(metadata, message);
        }

        void run() {
            std::deque<Entry> batch;
            while(true) {
                unsigned long long dropped_now = 0;
                bool stopping = false;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this] { return stop || !queue.empty() || dropped > 0; });
                    batch.swap(queue);
                    in_flight = batch.size();
                    dropped_now = dropped;
                    dropped = 0;
                    // Overflow is accounted as soon as the writer takes it,
                    // before I/O, under mtx so snapshot() sees it coherently.
                    dropped_total.fetch_add(dropped_now, std::memory_order_relaxed);
                    stopping = stop;
                }
                for(auto& entry : batch) {
                    write_inner(entry.first, entry.second);
                    std::lock_guard<std::mutex> guard(mtx);
                    if(in_flight > 0) --in_flight;
                    if(mode.load(std::memory_order_relaxed) == kAbandoned)
                        completed_after_abandon.fetch_add(1, std::memory_order_relaxed);
                }
                batch.clear();
                if(dropped_now > 0) {
                    AixLog::Metadata md;
                    md.severity = AixLog::Severity::warning;
                    md.timestamp = AixLog::Timestamp(std::chrono::system_clock::now());
                    write_inner(md, "async log queue full: dropped " + std::to_string(dropped_now) + " log lines");
                }
                if(stopping) {
                    std::lock_guard<std::mutex> guard(mtx);
                    if(queue.empty() && dropped == 0) {
                        finished = true;
                        done_cv.notify_all();
                        return;
                    }
                }
            }
        }
    };

    std::shared_ptr<State> state_;
    std::mutex lifecycle_mtx_;
    std::thread writer_;
};

#endif  // ASYNC_LOG_SINK_H
