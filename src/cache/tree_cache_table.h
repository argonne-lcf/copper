#ifndef TREE_CACHE_TABLE_H
#define TREE_CACHE_TABLE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../aixlog.h"
#include "cache_table.h"

using TreeCacheTableKey = std::string;
using TreeCacheTableVal = std::vector<std::string>;

class TreeCacheTable final : public CacheTable<TreeCacheTableKey, TreeCacheTableVal> {
    public:
    friend std::ostream& operator<<(std::ostream& os, const TreeCacheTable& tree_cache_table) {
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

    static void add_to_tree_cache(const std::string& path_string, bool is_file);
};


#endif // TREE_CACHE_TABLE_H
