#ifndef CACH_TABLES_H
#define CACH_TABLES_H

#include "data_cache_table.h"
#include "md_cache_table.h"
#include "tree_cache_table.h"

class CacheTables {
    public:
    static inline MDCacheTable md_cache_table;
    static inline DataCacheTable data_cache_table;
    static inline TreeCacheTable tree_cache_table;
};

#endif // CACH_TABLES_H
