#include "tree_cache_table.h"

// NOTE: including in header creates circular include
#include "cache_tables.h"

std::ostream& TreeCacheTable::get_data_size_metrics(std::ostream& os) {
    // Initialize the counter for the total number of bytes
    std::size_t num_bytes = 0;
    std::size_t num_elements = 0;

    for(const auto& entry : CacheTables::tree_cache_table.cache) {
        const auto& path = entry.first;

        std::size_t path_string_bytes = 0;
        for(const auto& path_string : entry.second) {
            path_string_bytes += path_string.size();
        }

        os << path << ": " << path_string_bytes << std::endl;
        num_bytes += path_string_bytes;
        num_elements++;
    }

    // Optionally print the total number of bytes
    os << "=============================================" << std::endl;
    os << "Total bytes: " << num_bytes << std::endl;
    os << "total number of entries: " << num_elements << std::endl;
    os << "=============================================" << std::endl;

    return os;
}

std::ostream& operator<<(std::ostream& os, const TreeCacheTable& tree_cache_table) {
    os << "tree_cache_table {" << std::endl;

    for(auto const& pair : tree_cache_table.cache) {
        os << pair.first << " {" << std::endl;

        for(auto const& child : pair.second) {
            os << child << "," << std::endl;
        }
        os << "}" << std::endl;
    }

    os << "}";

    return os;
}

void TreeCacheTable::add_to_tree_cache(const std::string& path_string, const bool is_file) {
    CacheTables::tree_cache_table.put_force(path_string, std::vector<std::string>());

    const std::filesystem::path path = path_string;
    const auto path_stem = path.stem().string();
    const auto parent_path = path.parent_path().string();
    const auto tree_entry = CacheTables::tree_cache_table.get(parent_path);

    if(!tree_entry.value()) {
        LOG(ERROR) << "failed to find parent path" << std::endl;
        return;
    }

    tree_entry.value()->push_back(path_stem);

    // NOTE: need to create new entry if it is a dir
    if(!is_file) {
        CacheTables::tree_cache_table.put_force(path_string, std::vector<std::string>());
    }
}
