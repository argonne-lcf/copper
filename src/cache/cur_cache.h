#ifndef CUR_CACHE_H
#define CUR_CACHE_H

#include "data_cache_table.h"
#include "md_cache_table.h"
#include "tree_cache_table.h"

class CurCache {
    public:
    static inline MDCacheTable md_cache_table;
    static inline DataCacheTable data_cache_table;
    static inline TreeCacheTable tree_cache_table;
};

#endif //CUR_CACHE_H
