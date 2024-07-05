#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <string>

#include "../fs/cu_stat.h"
#include "cache.h"

using MDCacheTableKey = std::string;
using MDCacheTableVal = CuStat;

class MDCacheTable final : public Cache<MDCacheTableKey, MDCacheTableVal> {
    public:
    static std::ostream& get_data_size_metrics(std::ostream& os);
    friend std::ostream& operator<<(std::ostream& os, const MDCacheTable& md_cache_table);
};

#endif // MDCACHETABLE_H
