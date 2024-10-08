#include "path_status_cache.h"

bool PathStatusCache::check_and_put_force(const Key& key) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if(entry == cache.end()) {
        cache.emplace(key, std::nullopt);
        return false;
    }

    return true;
}

void PathStatusCache::update_cache_status(const Key& key, const ssize_t status) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if(entry != cache.end()) {
        entry->second = status;
        cv.notify_all();
    }
}


int PathStatusCache::wait_on_cache_status(const Key& key) {
    std::unique_lock lock(mtx);

    auto pred = [&]() { return cache[key].has_value(); };
    cv.wait(lock, pred);

    return cache[key].value();
}
