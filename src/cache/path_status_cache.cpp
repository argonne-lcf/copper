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

void PathStatusCache::update_cache_status(const Key& key, const int status) {
    std::lock_guard guard(mtx);

    auto entry = cache.find(key);
    if(entry != cache.end()) {
        entry->second = status;
    }
}

int PathStatusCache::wait_on_cache_status(const Key& key) {
    for(;;) {
	{
		std::lock_guard guard(mtx);

		if(cache[key].has_value()) {
		    return cache[key].value();
		}
	}

        tl::thread::yield();
    }
}
