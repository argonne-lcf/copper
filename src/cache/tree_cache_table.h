#ifndef TREE_CACHE_TABLE_H
#define TREE_CACHE_TABLE_H

#include <filesystem>
#include <string>
#include <vector>

#include "../aixlog.h"
#include "cache.h"

using TreeCacheTableKey = std::string;
using TreeCacheTableVal = std::vector<std::string>;

class TreeCacheTable final : public Cache<TreeCacheTableKey, TreeCacheTableVal> {
    public:
    static std::ostream& get_data_size_metrics(std::ostream& os);
    friend std::ostream& operator<<(std::ostream& os, const TreeCacheTable& tree_cache_table);
    static void add_to_tree_cache(const std::string& path_string, bool is_file);
};


#endif // TREE_CACHE_TABLE_H
