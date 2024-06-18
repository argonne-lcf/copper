#ifndef DATA_CACHE_TABLE_H
#define DATA_CACHE_TABLE_H

#include <optional>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include "../aixlog.h"
#include "../fs/constants.h"
#include "cache_table.h"

using DataCacheTableKey = std::string;
using DataCacheTableVal = std::vector<std::byte>;

class DataCacheTable final : public CacheTable<DataCacheTableKey, DataCacheTableVal> {
    friend std::ostream& operator<<(std::ostream& os, const DataCacheTable& data_cache_table) {
        int cur_elements = 1;

        os << "data_cache_table {" << std::endl;

        for(auto const& entry : data_cache_table.cache) {
            os << entry.first << " {" << std::endl;
            os << "data_size: " << entry.second.size() << std::endl;
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

#endif // DATA_CACHE_TABLE_H
