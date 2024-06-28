#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <string>

#include "../fs/cu_stat.h"
#include "cache.h"

using MDCacheTableKey = std::string;
using MDCacheTableVal = CuStat;

class MDCacheTable final : public Cache<MDCacheTableKey, MDCacheTableVal> {
    friend std::ostream& operator<<(std::ostream& os, const MDCacheTable& md_cache_table) {
        os << "md_cache_table {" << std::endl;

        for(auto const& pair : md_cache_table.cache) {
            os << pair.first << " {" << std::endl;
            os << pair.second << std::endl;
            os << "}" << std::endl;
        }

        os << "}";

        return os;
    }
};

#endif // MDCACHETABLE_H
