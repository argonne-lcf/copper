#ifndef EVENTRECORD_H
#define EVENTRECORD_H

#include <ostream>
#include "operation_types.h"
#include "profiler_core.h"

class CacheEvent {
    public:
    // Legacy report facade; request events are owned by Operations/ProfilerCore.
    static std::ostream& log_data_cache_event(std::ostream& os);
    static std::ostream& log_tree_cache_event(std::ostream& os);
    static std::ostream& log_md_cache_event(std::ostream& os);
    static bool should_record_paths();
    static void reset_data_cache_event();
    static void reset_tree_cache_event();
    static void reset_md_cache_event();
};

#endif
