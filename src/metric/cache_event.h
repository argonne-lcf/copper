#ifndef EVENTRECORD_H
#define EVENTRECORD_H

#include <iostream>
#include <ostream>
#include <cstring>

#include "../aixlog.h"

class CacheEvent {
    public:
    static void record_data_cache_event(const std::string& path, bool cach_hit);
    static void record_dir_cache_event(const std::string& path, bool cach_hit);
    static void record_md_cache_event(const std::string& path, bool cach_hit);
    static std::ostream& log_data_cache_event(std::ostream& os);
    static std::ostream& log_dir_cache_event(std::ostream& os);
    static std::ostream& log_md_cache_event(std::ostream& os);

    private:
    // NOTE: pair.first is cache hits
    //       pair.second is cache misses
    //       first should never be > 1
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> data_cache_event_table;
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> dir_cache_event_table;
    static inline std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> md_cache_event_table;
};

#endif //EVENTRECORD_H
