#include "cache_event.h"

static void log_cache_event_helper(const std::string& table_name, std::ostream& os, std::unordered_map<std::string, OpRes>& event_cache_table) {
    os << table_name << " {" << std::endl;
    for(const auto& entry : event_cache_table) {
        os << entry.first << " {" << std::endl;
        os << "cache hits: " << entry.second.cache_hit << "," << std::endl;
        os << "cache misses: " << entry.second.cache_miss << "," << std::endl;
        os << "neg: " << entry.second.neg << std::endl;
        os << "}" << std::endl;
    }
    os << "}";
}

static void record_cache_event_helper(const std::string& path, std::unordered_map<std::string, OpRes>& event_cache_table, OperationResult res) {
    auto& event = event_cache_table[path];

    switch(res) {
    case(OperationResult::cache_hit): event.cache_hit++; break;
    case(OperationResult::cache_miss): event.cache_miss++; break;
    case(OperationResult::neg): event.neg++; break;
    default: LOG(ERROR) << "invalid operation result"; break;
    }
}

bool CacheEvent::should_record_paths() {
    return Constants::profile_paths_full || Constants::profile_top_n > 0;
}

void CacheEvent::record_cache_event(std::unordered_map<std::string, OpRes>& table, const std::string& path, OperationResult res) {
    if(!should_record_paths()) {
        return;
    }

    std::lock_guard<std::mutex> guard(cache_event_mtx);
    record_cache_event_helper(path, table, res);
}

std::ostream& CacheEvent::log_data_cache_event(std::ostream& os) {
    std::lock_guard<std::mutex> guard(cache_event_mtx);
    log_cache_event_helper("Data Cache Events", os, data_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_md_cache_event(std::ostream& os) {
    std::lock_guard<std::mutex> guard(cache_event_mtx);
    log_cache_event_helper("Metdata Cache Events", os, md_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_tree_cache_event(std::ostream& os) {
    std::lock_guard<std::mutex> guard(cache_event_mtx);
    log_cache_event_helper("Tree Cache Events", os, tree_cache_event_table);
    return os;
}
