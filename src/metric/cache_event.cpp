#include "cache_event.h"

static void log_cache_event_helper(const std::string& table_name,
std::ostream& os,
std::unordered_map<std::string, std::pair<unsigned int, unsigned int>>& event_cache_table) {
    os << Util::get_current_datetime() << std::endl;
#ifdef COLLECT_METRICS
    os << table_name << " {" << std::endl;
    for(const auto& entry : event_cache_table) {
        os << entry.first << " {" << std::endl;
        os << "cache hits | misses = " << entry.second.first << " | " << entry.second.second << std::endl;
        os << "}" << std::endl;
    }
    os << "}";
#endif
}

static void record_cache_event_helper(const std::string& path,
std::unordered_map<std::string, std::pair<unsigned int, unsigned int>>& event_cache_table,
bool cache_hit) {
#ifdef COLLECT_METRICS
    std::pair<unsigned int, unsigned int>* event = nullptr;

    try {
        event = &event_cache_table.at(path);
    } catch(std::exception& e) {
        event_cache_table.emplace(path, std::make_pair(0, 0));
        event = &event_cache_table.at(path);
    }

    if(cache_hit) {
        event->first++;
    } else {
        event->second++;
    }
#endif
}

void CacheEvent::record_data_cache_event(const std::string& path, const bool cach_hit) {
    record_cache_event_helper(path, data_cache_event_table, cach_hit);
}

void CacheEvent::record_dir_cache_event(const std::string& path, const bool cach_hit) {
    record_cache_event_helper(path, dir_cache_event_table, cach_hit);
}

void CacheEvent::record_md_cache_event(const std::string& path, const bool cach_hit) {
    record_cache_event_helper(path, md_cache_event_table, cach_hit);
}

std::ostream& CacheEvent::log_data_cache_event(std::ostream& os) {
    log_cache_event_helper("data_cache_event_table", os, data_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_md_cache_event(std::ostream& os) {
    log_cache_event_helper("md_cache_event_table", os, md_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_dir_cache_event(std::ostream& os) {
    log_cache_event_helper("dir_cache_event_table", os, dir_cache_event_table);
    return os;
}