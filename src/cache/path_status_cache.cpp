#include "path_status_cache.h"

bool PathStatusCache::check_and_put_force(const Key& key) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if(entry == cache.end()) {
        LOG(DEBUG) << "path-status insert fresh key: " << key << std::endl;
        cache.emplace(key, std::nullopt);
        return false;
    }

    // Once a lookup has completed, the path-status entry should no longer
    // block a fresh request. Keep this table as "in-flight coordination"
    // state instead of a permanent negative/positive result cache.
    if(entry->second.has_value()) {
        LOG(DEBUG) << "path-status stale completed entry, allowing fresh lookup for key: " << key
                   << " previous_status=" << entry->second.value() << std::endl;
        entry->second = std::nullopt;
        return false;
    }

    LOG(TRACE) << "path-status key already in flight: " << key << std::endl;
    return true;
}

void PathStatusCache::update_cache_status(const Key& key, const ssize_t status) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if(entry != cache.end()) {
        LOG(DEBUG) << "path-status update key: " << key << " status=" << status << std::endl;
        entry->second = status;
        cv.notify_all();
    }
}


int PathStatusCache::wait_on_cache_status(const Key& key) {
    std::unique_lock lock(mtx);

    LOG(TRACE) << "path-status waiting on key: " << key << std::endl;
    auto pred = [&]() { return cache[key].has_value(); };
    cv.wait(lock, pred);

    LOG(DEBUG) << "path-status wake key: " << key << " status=" << cache[key].value() << std::endl;
    return cache[key].value();
}
