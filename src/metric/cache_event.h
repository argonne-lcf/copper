#ifndef EVENTRECORD_H
#define EVENTRECORD_H

#include <cstring>
#include <iostream>
#include <ostream>

#include "metrics.h"
#include "../aixlog.h"

class CacheEvent {
    public:
    static void record_data_cache_event(const std::string& path, bool cach_hit);
    static void record_dir_cache_event(const std::string& path, bool cach_hit);
    static void record_md_cache_event(const std::string& path, bool cach_hit);

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

    private:
    // NOTE: pair.first is cache hits
    //       pair.second is cache misses
    //       first should never be > 1
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> data_cache_event_table;
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> dir_cache_event_table;
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> md_cache_event_table;
};

#endif // EVENTRECORD_H
