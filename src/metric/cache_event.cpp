#include "cache_event.h"
#include "operations.h"

namespace {
std::ostream& log_source(std::ostream& os, ProfileSource source, const char* name) {
    const auto snapshot = Operations::snapshot();
    os << name << " {\n";
    for(const auto& event : snapshot.path_events) {
        if(event.source != source) continue;
        os << event.path << " {\n"
           << "cache hits: " << event.outcomes[static_cast<size_t>(ProfileOutcome::cache_hit)] << ",\n"
           << "cache misses: " << event.outcomes[static_cast<size_t>(ProfileOutcome::cache_miss)] << ",\n"
           << "neg: " << event.outcomes[static_cast<size_t>(ProfileOutcome::negative)] << "\n}\n";
    }
    return os << '}';
}
} // namespace

bool CacheEvent::should_record_paths() { return Operations::config().full_paths || Operations::config().top_n > 0; }
std::ostream& CacheEvent::log_data_cache_event(std::ostream& os) { return log_source(os, ProfileSource::data, "Data Cache Events"); }
std::ostream& CacheEvent::log_tree_cache_event(std::ostream& os) { return log_source(os, ProfileSource::tree, "Tree Cache Events"); }
std::ostream& CacheEvent::log_md_cache_event(std::ostream& os) { return log_source(os, ProfileSource::metadata, "Metadata Cache Events"); }
// Compatibility clear requests reset the coherent epoch rather than partially
// clearing one dimension and creating internally inconsistent counters.
void CacheEvent::reset_data_cache_event() { Operations::snapshot_and_reset(); }
void CacheEvent::reset_tree_cache_event() { Operations::snapshot_and_reset(); }
void CacheEvent::reset_md_cache_event() { Operations::snapshot_and_reset(); }
