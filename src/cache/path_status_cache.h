#ifndef PATH_STATUS_CACHE_H
#define PATH_STATUS_CACHE_H

#include <condition_variable>
#include <string>

#include "cache.h"

using PathStatusCacheTableKey = std::string;
using PathStatusMDCacheTableVal = std::optional<int>;

class PathStatusCache final : public Cache<PathStatusCacheTableKey, PathStatusMDCacheTableVal> {
    public:
    bool check_and_put_force(const Key& key);
    bool update_cache_status(const Key&, int status);
    int wait_on_cache_status(const Key& key);

    private:
    std::condition_variable cv;
};

#endif // PATH_STATUS_CACHE_H
