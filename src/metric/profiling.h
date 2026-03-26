#ifndef CU_FUSE_PROFILING_H
#define CU_FUSE_PROFILING_H

#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>

#include "../fs/constants.h"
#include "cache_event.h"
#include "operation_types.h"
#include "operations.h"

struct MDTTLEvent {
    uint64_t store = 0;
    uint64_t serve = 0;
    uint64_t expire = 0;
    uint64_t clear = 0;
};

class Profiling {
    public:
    static bool enabled();
    static bool top_paths_enabled();
    static bool full_paths_enabled();

    static void record_md_ttl_store(const std::string& path);
    static void record_md_ttl_serve(const std::string& path);
    static void record_md_ttl_expire(const std::string& path);
    static void record_md_ttl_clear(const std::string& path);

    static uint64_t get_md_ttl_store_count();
    static uint64_t get_md_ttl_serve_count();
    static uint64_t get_md_ttl_expire_count();
    static uint64_t get_md_ttl_clear_count();

    static std::ostream& log_md_ttl_event(std::ostream& os);
    static void write_reports(const std::string& path_string, const std::string& snapshot_tag = "");
    static void reset();
    static std::string mode_string();

    private:
    static std::string tagged_filename(const std::string& base_filename, const std::string& snapshot_tag);
    static void write_operations_csv(const std::string& output_path);
    static void write_top_paths_csv(const std::string& output_path);
    static void write_summary_md(const std::string& output_path, const std::string& snapshot_tag);
    static void write_aggregate_csv(const std::string& output_path);
    static void write_full_path_outputs(const std::string& path_string, const std::string& snapshot_tag);
    static void write_path_guidance_md(std::ostream& os);

    static inline std::unordered_map<std::string, MDTTLEvent> md_ttl_event_table;
    static inline uint64_t md_ttl_store_count = 0;
    static inline uint64_t md_ttl_serve_count = 0;
    static inline uint64_t md_ttl_expire_count = 0;
    static inline uint64_t md_ttl_clear_count = 0;
    static inline std::mutex profiling_mtx;
};

#endif // CU_FUSE_PROFILING_H
