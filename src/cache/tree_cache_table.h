#ifndef TREE_CACHE_TABLE_H
#define TREE_CACHE_TABLE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../fs/constants.h"
#include "cache_table.h"

using TreeCacheTableKey = std::string;
using TreeCacheTableVal = std::vector<std::string>;

class TreeCacheTable final : public CacheTable<TreeCacheTableKey, TreeCacheTableVal> {
    public:
    friend std::ostream& operator<<(std::ostream& os, const TreeCacheTable& tree_cache_table) {
        int cur_elements = 1;

        os << "tree_cache_table {" << std::endl;

        for(auto const& pair : tree_cache_table.cache) {
            os << pair.first << " {" << std::endl;

            for(auto const& child : pair.second) {
                os << child << "," << std::endl;
            }
            os << "}" << std::endl;

            if(cur_elements++ >= Constants::max_print_cache_table_entries) {
                os << "..." << std::endl;
                break;
            }
        }

        os << "}";

        return os;
    }

    static void add_to_tree_cache(const std::string& path_string, bool is_file);
};


#endif // TREE_CACHE_TABLE_H
