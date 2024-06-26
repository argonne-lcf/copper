#include "tree_cache_table.h"

// NOTE: including in header creates circular include
#include "cache_tables.h"

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
