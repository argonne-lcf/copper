#include "data_cache_table.h"

#include "../fs/util.h"
#include "cache_tables.h"

std::ostream& DataCacheTable::get_data_size_metrics(std::ostream& os) {
    const auto snap = CacheTables::data_cache_table.snapshot();
    std::vector<std::pair<std::string, std::vector<std::byte>>> entries(snap.begin(), snap.end());

    // Sort the vector by the size of the data (second element of the pair)
    std::sort(
    entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.second.size() < b.second.size(); });

    // Initialize the counter for the total number of bytes
    std::size_t num_bytes = 0;
    std::size_t num_elements = 0;

    // Print the entries in sorted order
    for(const auto& entry : entries) {
        const auto& path = entry.first;
        const auto& data = entry.second;

        os << path << ": " << data.size() << std::endl;

        num_elements++;
        num_bytes += data.size();
    }

    // Optionally print the total number of bytes
    os << "=============================================" << std::endl;
    os << "Total bytes: " << num_bytes << std::endl;
    os << "total number of files: " << num_elements << std::endl;
    os << "=============================================" << std::endl;

    return os;
}

std::ostream& operator<<(std::ostream& os, const DataCacheTable& data_cache_table) {
    os << "data_cache_table {" << std::endl;
    const auto snap = data_cache_table.snapshot();
    for(auto const& entry : snap) {
        os << entry.first << " {" << std::endl;
        os << "data_size: " << entry.second.size() << std::endl;
        os << "}" << std::endl;
    }
    os << "}";

    return os;
}