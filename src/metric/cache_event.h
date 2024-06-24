#ifndef EVENTRECORD_H
#define EVENTRECORD_H

#include <cstring>
#include <iostream>
#include <ostream>
#include <unordered_map>

#include "../aixlog.h"
#include "../fs/util.h"
#include "../metric/cache_event.h"
#include "metrics.h"
#include "operation_types.h"

class CacheEvent {
    public:
    static void record_cache_event(std::unordered_map<std::string, OpRes>& table, const std::string& path, OperationResult res);

    static std::ostream& log_data_cache_event(std::ostream& os);
    static std::ostream& log_dir_cache_event(std::ostream& os);
    static std::ostream& log_md_cache_event(std::ostream& os);

    static void reset_data_cache_event() {
        data_cache_event_table.clear();
    }
    static void reset_dir_cache_event() {
        dir_cache_event_table.clear();
    }
    static void reset_md_cache_event() {
        md_cache_event_table.clear();
    }

    // NOTE: pair.first is cache hits
    //       pair.second is cache misses
    //       first should never be > 1
    static inline std::unordered_map<std::string, OpRes> data_cache_event_table;
    static inline std::unordered_map<std::string, OpRes> dir_cache_event_table;
    static inline std::unordered_map<std::string, OpRes> md_cache_event_table;
};

#endif // EVENTRECORD_H
