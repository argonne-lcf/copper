#ifndef DATA_CACHE_TABLE_H
#define DATA_CACHE_TABLE_H

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache.h"

using DataCacheTableKey = std::string;
using DataCacheTableVal = std::vector<std::byte>;

class DataCacheTable final : public Cache<DataCacheTableKey, DataCacheTableVal> {
    public:
    static std::ostream& get_data_size_metrics(std::ostream& os);

    friend std::ostream& operator<<(std::ostream& os, const DataCacheTable& data_cache_table) {
        os << "data_cache_table {" << std::endl;
        for(auto const& entry : data_cache_table.cache) {
            os << entry.first << " {" << std::endl;
            os << "data_size: " << entry.second.size() << std::endl;
            os << "}" << std::endl;
        }
        os << "}";

        return os;
    }
};

#endif // DATA_CACHE_TABLE_H
