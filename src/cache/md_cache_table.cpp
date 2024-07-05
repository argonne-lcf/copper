#include "md_cache_table.h"

#include "cache_tables.h"

std::ostream& MDCacheTable::get_data_size_metrics(std::ostream& os) {
    // Initialize the counter for the total number of bytes
    std::size_t num_bytes = 0;
    std::size_t num_elements = 0;

    os << "all entries should be 144 bytes" << std::endl;
    for(const auto& entry : CacheTables::md_cache_table.cache) {
        const auto& path = entry.first;

        os << path << ": " << entry.second.get_st_vec_cpy().size() << std::endl;

        num_bytes += 144;
        num_elements++;
    }

    // Optionally print the total number of bytes
    os << "=============================================" << std::endl;
    os << "Total bytes: " << num_bytes << std::endl;
    os << "total number of entries: " << num_elements << std::endl;
    os << "=============================================" << std::endl;

    return os;
}

std::ostream& operator<<(std::ostream& os, const MDCacheTable& md_cache_table) {
    os << "md_cache_table {" << std::endl;

    for(auto const& pair : md_cache_table.cache) {
        os << pair.first << " {" << std::endl;
        os << pair.second << std::endl;
        os << "}" << std::endl;
    }

    os << "}";

    return os;
}