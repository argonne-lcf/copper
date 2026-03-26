#ifndef CACH_TABLES_H
#define CACH_TABLES_H

#include "data_cache_table.h"
#include "md_cache_table.h"
#include "path_status_cache.h"
#include "tree_cache_table.h"

class CacheTables {
    public:
    static inline MDCacheTable md_cache_table;
    static inline DataCacheTable data_cache_table;
    static inline TreeCacheTable tree_cache_table;

    static inline PathStatusCache md_path_status_cache_table;
    static inline PathStatusCache data_path_status_cache_table;
    static inline PathStatusCache tree_path_status_cache_table;
};

#endif // CACH_TABLES_H
