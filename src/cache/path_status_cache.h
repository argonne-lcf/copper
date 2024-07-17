#ifndef PATH_STATUS_CACHE_H
#define PATH_STATUS_CACHE_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>
#include <condition_variable>
#include <thallium.hpp>

namespace tl = thallium;

#include "../aixlog.h"
#include "tl_cache.h"

using PathStatusCacheTableKey = std::string;
using PathStatusMDCacheTableVal = std::optional<int>;

class PathStatusCache final : public TLCache<PathStatusCacheTableKey, PathStatusMDCacheTableVal> {
    public:
    bool check_and_put_force(const PathStatusCacheTableKey& key);
    void update_cache_status(const PathStatusCacheTableKey& key, int status);
    int wait_on_cache_status(const PathStatusCacheTableKey& key);

    private:
    std::unordered_map<PathStatusCacheTableKey, PathStatusMDCacheTableVal> cache;
    std::unordered_map<PathStatusCacheTableKey, tl::condition_variable> cvs;
    tl::mutex mtx;
};

#endif // PATH_STATUS_CACHE_H
