#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <string>
#include <sys/stat.h>

#include "../fs/constants.h"
#include "../fs/cu_stat.h"
#include "cache_table.h"

using MDCacheTableKey = std::string;
using MDCacheTableVal = CuStat;

class MDCacheTable final : public CacheTable<MDCacheTableKey, MDCacheTableVal> {
    friend std::ostream& operator<<(std::ostream& os, const MDCacheTable& md_cache_table) {
        int cur_elements = 1;

        os << "md_cache_table {" << std::endl;

        for(auto const& pair : md_cache_table.cache) {
            os << pair.first << " {" << std::endl;
            os << pair.second << std::endl;
            os << "}" << std::endl;

            if(cur_elements++ >= Constants::max_print_cache_table_entries) {
                os << "..." << std::endl;
                break;
            }
        }

        os << "}";

        return os;
    }
};

#endif // MDCACHETABLE_H
