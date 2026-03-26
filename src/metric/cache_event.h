#ifndef EVENTRECORD_H
#define EVENTRECORD_H

#include <cstring>
#include <iostream>
#include <mutex>
#include <ostream>
#include <unordered_map>

#include "../aixlog.h"
#include "../fs/util.h"
#include "../metric/cache_event.h"
#include "operation_types.h"

class CacheEvent {
    public:
    static void record_cache_event(std::unordered_map<std::string, OpRes>& table, const std::string& path, OperationResult res);

    static std::ostream& log_data_cache_event(std::ostream& os);
    static std::ostream& log_tree_cache_event(std::ostream& os);
    static std::ostream& log_md_cache_event(std::ostream& os);
    static bool should_record_paths();

    static void reset_data_cache_event() {
        std::lock_guard<std::mutex> guard(cache_event_mtx);
        data_cache_event_table.clear();
    }
    static void reset_tree_cache_event() {
        std::lock_guard<std::mutex> guard(cache_event_mtx);
        tree_cache_event_table.clear();
    }
    static void reset_md_cache_event() {
        std::lock_guard<std::mutex> guard(cache_event_mtx);
        md_cache_event_table.clear();
    }

    // NOTE: pair.first is cache hits
    //       pair.second is cache misses
    //       first should never be > 1
    static inline std::unordered_map<std::string, OpRes> data_cache_event_table;
    static inline std::unordered_map<std::string, OpRes> tree_cache_event_table;
    static inline std::unordered_map<std::string, OpRes> md_cache_event_table;
    static inline std::mutex cache_event_mtx;
};

#endif // EVENTRECORD_H
