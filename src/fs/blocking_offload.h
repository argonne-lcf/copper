#ifndef COPPER_BLOCKING_OFFLOAD_H
#define COPPER_BLOCKING_OFFLOAD_H

// Runs blocking filesystem calls (lstat, opendir/readdir, open/read) for
// Thallium RPC handlers on a small pool of dedicated OS worker threads.
//
// Why: cu_fuse runs every RPC handler ULT on one Argobots execution stream by
// default (-es 1). A plain syscall in a handler parks that OS thread, so one
// slow or hung storage call (a stuck Lustre OST, an unreachable server) stalls
// every other RPC on the node, including readiness probes, for as long as the
// call takes, with no limit.
//
// With the offload, the handler ULT only enqueues the job and then waits by
// calling `wait_step` (in production tl::thread::sleep, which yields the ULT),
// so the execution stream keeps serving other RPCs. A syscall cannot be
// cancelled, so a timed-out worker keeps running it; the job's state is shared
// (shared_ptr) and is freed when the worker finishes, whether or not anyone is
// still waiting.
//
// Deadline policy (review r7 finding 1): COMPLETION-TIME. A result is accepted
// only if the worker finished the call at or before `deadline_ms`; the worker
// records its completion time. A call that finishes after the deadline is
// timed_out even if the caller only notices later, and an already-expired
// deadline returns timed_out without submitting anything. What is NOT bounded
// by the deadline: how late the caller observes the outcome (cooperative
// rescheduling delay), the short pool-mutex section in submit/cancel, worker
// thread creation (at most `threads` times per process), and job allocation.
// The queue cap bounds the number of waiting jobs, not their byte size: a
// running read or directory listing allocates whatever the file or directory
// needs.
//
// Errors (review r7 finding 2): run() never throws. Exceptions from `fn`, and
// any failure while submitting (allocation, thread creation, a test hook), are
// returned as Outcome::failed with a message. status_of() maps every outcome
// to an errno-style status. Preparing a call (acquiring the pool, building
// the closure) happens outside run(); see RootStorage::guarded_reply for the
// boundary that covers that.
//
// Bounds (counts, not latency or bytes; see the deadline policy above):
//  * At most `threads` blocking calls run at once (storage load is bounded).
//  * At most `queue_capacity` jobs wait for a worker; beyond that run()
//    returns rejected immediately. This bounds the number of queued jobs, not
//    their memory: a running read or listing allocates what the file or
//    directory needs.
//  * If every worker is stuck in a hung call, queued jobs are removed at their
//    own deadlines and new jobs are rejected once the queue is full. A result
//    is accepted only if the call completed by its deadline; how soon the
//    caller notices depends on when its ULT is rescheduled.
//
// Fork: workers start lazily on the first run(), which happens in the daemon
// after fuse_main() has forked (handlers only exist after cu_fuse_init).
// Workers are detached and co-own the pool state, so the pool object can be
// destroyed (or the process can exit) while a worker is still blocked.

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace CopperOffload {

enum class Outcome { done, timed_out, rejected, failed };

inline long long steady_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class BlockingOffload final {
    public:
    BlockingOffload(std::size_t threads, std::size_t queue_capacity)
    : state_(std::make_shared<State>()) {
        state_->threads = threads == 0 ? 1 : threads;
        state_->queue_capacity = queue_capacity;
    }
    // Test-only: called inside submit() before the job is queued; may throw to
    // simulate an allocation or thread-creation failure during submission.
    void set_submit_hook_for_test(std::function<void()> hook) { state_->submit_hook = std::move(hook); }

    BlockingOffload(const BlockingOffload&) = delete;
    BlockingOffload& operator=(const BlockingOffload&) = delete;

    ~BlockingOffload() { shutdown(); }

    // Runs `fn` on a worker and waits until it completes or `deadline_ms`
    // (steady_now_ms() clock) passes. `wait_step(attempt)` is called between
    // polls and must yield the caller (tl::thread::sleep in a handler ULT).
    // On done, `result` holds fn's return value. Otherwise `result` is left
    // empty, fn's eventual result (if any) is discarded, and for failed
    // `error` (if given) describes the failure. Never throws.
    template <typename R>
    Outcome run(const std::function<R()>& fn, std::optional<R>& result, long long deadline_ms,
                const std::function<void(int attempt)>& wait_step, std::string* error = nullptr) noexcept {
        auto set_error = [error](const char* what) noexcept {
            if(error == nullptr) return;
            try {
                *error = what;
            } catch(...) {
            }
        };
        try {
            if(steady_now_ms() >= deadline_ms) {  // pre-expired: do not start the call
                state_->timed_out.fetch_add(1, std::memory_order_relaxed);
                return Outcome::timed_out;
            }
            struct Job final : JobBase {
                std::function<R()> fn;
                std::optional<R> value;
                std::exception_ptr error;
                void execute() noexcept override {
                    try {
                        value.emplace(fn());
                    } catch(...) {
                        error = std::current_exception();
                    }
                }
            };
            auto job = std::make_shared<Job>();
            job->fn = fn;
            if(!submit(job)) return Outcome::rejected;
            for(int attempt = 0;; ++attempt) {
                if(job->complete.load(std::memory_order_acquire)) {
                    if(job->completed_at_ms > deadline_ms) {  // finished, but too late
                        state_->timed_out.fetch_add(1, std::memory_order_relaxed);
                        return Outcome::timed_out;
                    }
                    if(job->error) {
                        try {
                            std::rethrow_exception(job->error);
                        } catch(const std::exception& e) {
                            set_error(e.what());
                        } catch(...) {
                            set_error("non-standard exception");
                        }
                        state_->failed.fetch_add(1, std::memory_order_relaxed);
                        return Outcome::failed;
                    }
                    result = std::move(job->value);
                    return Outcome::done;
                }
                if(steady_now_ms() >= deadline_ms) {
                    // Leave the job to the worker (or drop it from the queue if no
                    // worker took it yet); the shared_ptr keeps it alive either way.
                    cancel_if_queued(job);
                    state_->timed_out.fetch_add(1, std::memory_order_relaxed);
                    return Outcome::timed_out;
                }
                wait_step(attempt);
            }
        } catch(const std::exception& e) {  // submission/allocation/wait_step failure
            set_error(e.what());
        } catch(...) {
            set_error("non-standard exception");
        }
        state_->failed.fetch_add(1, std::memory_order_relaxed);
        return Outcome::failed;
    }

    // Stops idle workers. Workers blocked in a call exit once it returns.
    void shutdown() {
        {
            std::lock_guard<std::mutex> g(state_->mtx);
            state_->stop = true;
        }
        state_->cv.notify_all();
    }

    unsigned long long rejected_total() const { return state_->rejected.load(); }
    unsigned long long timed_out_total() const { return state_->timed_out.load(); }
    unsigned long long failed_total() const { return state_->failed.load(); }
    unsigned long long started_workers() const {
        std::lock_guard<std::mutex> g(state_->mtx);
        return state_->started;
    }
    unsigned long long busy_workers() const { return state_->busy.load(); }

    private:
    struct JobBase {
        std::atomic<bool> complete{false};
        std::atomic<bool> cancelled{false};
        long long completed_at_ms = 0;  // written before `complete` (release)
        virtual ~JobBase() = default;
        virtual void execute() noexcept = 0;
    };

    struct State {
        std::size_t threads = 1;
        std::size_t queue_capacity = 0;
        mutable std::mutex mtx;
        std::condition_variable cv;
        std::deque<std::shared_ptr<JobBase>> queue;
        std::size_t started = 0;
        bool stop = false;
        std::atomic<unsigned long long> busy{0};
        std::atomic<unsigned long long> rejected{0};
        std::atomic<unsigned long long> timed_out{0};
        std::atomic<unsigned long long> failed{0};
        std::function<void()> submit_hook;

        void worker() {
            while(true) {
                std::shared_ptr<JobBase> job;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this] { return stop || !queue.empty(); });
                    if(queue.empty()) return;  // stop requested and nothing left
                    job = std::move(queue.front());
                    queue.pop_front();
                    if(job->cancelled.load(std::memory_order_relaxed)) continue;
                    busy.fetch_add(1, std::memory_order_relaxed);
                }
                job->execute();  // may block for a long time; never under mtx
                job->completed_at_ms = steady_now_ms();
                job->complete.store(true, std::memory_order_release);
                busy.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    };

    // Short OS-mutex critical section: queue push and, at most `threads` times
    // per process, std::thread creation. No I/O under the lock.
    bool submit(const std::shared_ptr<JobBase>& job) {
        State& st = *state_;
        if(st.submit_hook) st.submit_hook();
        {
            std::lock_guard<std::mutex> g(st.mtx);
            if(st.stop || st.queue.size() >= st.queue_capacity) {
                st.rejected.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            st.queue.push_back(job);
            // Start another worker while every started one may be busy.
            if(st.started < st.threads && st.started <= st.busy.load() + st.queue.size() - 1) {
                std::shared_ptr<State> keep = state_;
                try {
                    std::thread([keep] { keep->worker(); }).detach();
                    ++st.started;
                } catch(const std::system_error&) {
                    if(st.started == 0) {  // no worker at all: undo, fail fast
                        st.queue.pop_back();
                        st.rejected.fetch_add(1, std::memory_order_relaxed);
                        return false;
                    }
                }
            }
        }
        st.cv.notify_one();
        return true;
    }

    void cancel_if_queued(const std::shared_ptr<JobBase>& job) {
        job->cancelled.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> g(state_->mtx);
        for(auto it = state_->queue.begin(); it != state_->queue.end(); ++it) {
            if(*it == job) {
                state_->queue.erase(it);
                return;
            }
        }
    }

    std::shared_ptr<State> state_;
};

// errno-style status for an outcome: 0 done, -ETIMEDOUT, -EAGAIN, -EIO.
inline int status_of(Outcome o) noexcept {
    switch(o) {
        case Outcome::done: return 0;
        case Outcome::timed_out: return -ETIMEDOUT;
        case Outcome::rejected: return -EAGAIN;
        case Outcome::failed: return -EIO;
    }
    return -EIO;
}

}  // namespace CopperOffload

#endif  // COPPER_BLOCKING_OFFLOAD_H
