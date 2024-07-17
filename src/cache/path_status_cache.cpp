#include "path_status_cache.h"

bool PathStatusCache::check_and_put_force(const PathStatusCacheTableKey& key) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if (entry == cache.end()) {
        cache.emplace(key, std::nullopt);
        cvs.emplace(key, tl::condition_variable());
        return false;
    }

    return true;
}

void PathStatusCache::update_cache_status(const PathStatusCacheTableKey& key, int status) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if (entry != cache.end()) {
        entry->second = status;
        if (cvs.find(key) != cvs.end()) {
            cvs[key].notify_all();
        }
    }
}

int PathStatusCache::wait_on_cache_status(const PathStatusCacheTableKey& key) {
    std::unique_lock lock(mtx);

    auto pred = [&]() { return cache[key].has_value(); };
    if (cvs.find(key) != cvs.end()) {
        cvs[key].wait(lock, pred);
    }

    return cache[key].value();
}
