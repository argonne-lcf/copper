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

void CacheEvent::record_cache_event(std::unordered_map<std::string, OpRes>& table, const std::string& path, OperationResult res) {
    record_cache_event_helper(path, table, res);
}

std::ostream& CacheEvent::log_data_cache_event(std::ostream& os) {
    log_cache_event_helper("Data Cache Events", os, data_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_md_cache_event(std::ostream& os) {
    log_cache_event_helper("Metdata Cache Events", os, md_cache_event_table);
    return os;
}

std::ostream& CacheEvent::log_tree_cache_event(std::ostream& os) {
    log_cache_event_helper("Tree Cache Events", os, tree_cache_event_table);
    return os;
}