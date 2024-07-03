#include "data_cache_table.h"

#include "cache_tables.h"

std::ostream& DataCacheTable::get_data_size_metrics(std::ostream& os) {
    unsigned int num_bytes = 0;

    for(const auto& entry: CacheTables::data_cache_table.cache) {
        const auto& path = entry.first;
        const auto& data = entry.second;

        os << path << ": " << data.size() << std::endl;

        num_bytes += data.size();
    }

    os << "=============================================" << std::endl;
    os << "total data cache size: " << num_bytes;

    return os;
}