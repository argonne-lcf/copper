#ifndef ABT_SYNC_H
#define ABT_SYNC_H

#include <abt.h>
#include <cstdio>
#include <cstdlib>

// Argobots-aware mutex/condition variable for state shared by Thallium RPC
// handler ULTs. Unlike std::mutex/std::condition_variable, waiting yields the
// ULT instead of parking the whole execution stream, which matters because
// cu_fuse runs every RPC handler on a single stream by default (-es 1).
//
// Uses statically initialised ABT_*_memory, so objects can be static/global
// (constructed before ABT_init) without calling ABT_mutex_create/ABT_cond_create.
// ABT sync calls on statically initialised objects cannot fail in a correct
// program; a non-success code means misuse (e.g. unlock without lock), so abort
// loudly rather than continue with corrupted shared state.
inline void abt_sync_check(int rc, const char* what) {
    if(rc != ABT_SUCCESS) {
        std::fprintf(stderr, "copper: %s failed with Argobots rc=%d\n", what, rc);
        std::abort();
    }
}

class AbtMutex final {
    public:
    AbtMutex() = default;
    AbtMutex(const AbtMutex&) = delete;
    AbtMutex& operator=(const AbtMutex&) = delete;
    void lock() { abt_sync_check(ABT_mutex_lock(native()), "ABT_mutex_lock"); }
    bool try_lock() {
        const int rc = ABT_mutex_trylock(native());
        if(rc == ABT_ERR_MUTEX_LOCKED) return false;
        abt_sync_check(rc, "ABT_mutex_trylock");
        return true;
    }
    void unlock() { abt_sync_check(ABT_mutex_unlock(native()), "ABT_mutex_unlock"); }
    ABT_mutex native() const { return ABT_MUTEX_MEMORY_GET_HANDLE(&mem_); }
    private:
    mutable ABT_mutex_memory mem_ = ABT_MUTEX_INITIALIZER;
};

class AbtCondition final {
    public:
    AbtCondition() = default;
    AbtCondition(const AbtCondition&) = delete;
    AbtCondition& operator=(const AbtCondition&) = delete;
    // Caller must hold `m`; it is released while waiting and re-acquired.
    template <typename Predicate> void wait(AbtMutex& m, Predicate pred) {
        while(!pred()) abt_sync_check(ABT_cond_wait(native(), m.native()), "ABT_cond_wait");
    }
    void notify_all() { abt_sync_check(ABT_cond_broadcast(native()), "ABT_cond_broadcast"); }
    ABT_cond native() const { return ABT_COND_MEMORY_GET_HANDLE(&mem_); }
    private:
    mutable ABT_cond_memory mem_ = ABT_COND_INITIALIZER;
};

#endif // ABT_SYNC_H
