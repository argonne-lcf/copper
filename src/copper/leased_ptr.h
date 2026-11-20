#ifndef COPPER_LEASED_PTR_H
#define COPPER_LEASED_PTR_H

// Owner of a heap object that concurrent users borrow through counted leases.
// close() stops new leases, waits (bounded) until no lease is held, and only
// then frees the object. If the wait times out, the object is leaked, never
// freed while a lease is live.
//
// Used for the parent tl::endpoint (review r4 finding 2): RPC handler ULTs on
// other nodes' behalf may still be using it when cu_fuse_destroy runs.
//
// Lease acquire/release only take a short std::mutex section with no waits,
// so they are safe on Argobots handler ULTs. close() sleeps on the OS thread
// and must be called from a non-handler thread (cu_fuse_destroy runs on a
// FUSE OS thread).

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

template <typename T> class LeasedPtr {
    public:
    class Lease {
        public:
        explicit Lease(LeasedPtr& owner) : owner_(owner) {
            std::lock_guard<std::mutex> guard(owner_.mtx_);
            if(owner_.closed_) throw std::runtime_error("resource closed: shutdown in progress");
            if(!owner_.ptr_) throw std::runtime_error("resource not initialized");
            obj_ = owner_.ptr_.get();
            ++owner_.users_;
        }
        ~Lease() {
            std::lock_guard<std::mutex> guard(owner_.mtx_);
            --owner_.users_;
        }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        const T& get() const { return *obj_; }

        private:
        LeasedPtr& owner_;
        const T* obj_ = nullptr;
    };

    void reset(std::unique_ptr<T> p) {
        std::lock_guard<std::mutex> guard(mtx_);
        ptr_ = std::move(p);
        closed_ = false;
    }

    bool has_value() const {
        std::lock_guard<std::mutex> guard(mtx_);
        return ptr_ != nullptr && !closed_;
    }

    int users() const {
        std::lock_guard<std::mutex> guard(mtx_);
        return users_;
    }

    // Returns true if the object was freed with no lease outstanding, false on
    // timeout (object leaked). Idempotent.
    bool close(std::chrono::milliseconds timeout) {
        {
            std::lock_guard<std::mutex> guard(mtx_);
            closed_ = true;
        }
        const auto end = std::chrono::steady_clock::now() + timeout;
        while(true) {
            {
                std::lock_guard<std::mutex> guard(mtx_);
                if(users_ == 0) {
                    ptr_.reset();
                    return true;
                }
                if(std::chrono::steady_clock::now() >= end) {
                    (void)ptr_.release();  // never free under a live lease
                    return false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    private:
    mutable std::mutex mtx_;
    std::unique_ptr<T> ptr_;
    bool closed_ = false;
    int users_ = 0;
};

#endif // COPPER_LEASED_PTR_H
