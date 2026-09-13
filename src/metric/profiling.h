#ifndef CU_FUSE_PROFILING_H
#define CU_FUSE_PROFILING_H

#include <cstdint>
#include <ostream>
#include <string>

#include "../fs/constants.h"
#include "operations.h"


class Profiling {
    public:
    static bool enabled();
    static bool top_paths_enabled();
    static bool full_paths_enabled();
    static std::string mode_string();

    static void record_md_ttl_store(const std::string& path);
    static void record_md_ttl_serve(const std::string& path);
    static void record_md_ttl_expire(const std::string& path);
    static void record_md_ttl_clear(const std::string& path);
    static uint64_t get_md_ttl_store_count();
    static uint64_t get_md_ttl_serve_count();
    static uint64_t get_md_ttl_expire_count();
    static uint64_t get_md_ttl_clear_count();
    static std::ostream& log_md_ttl_event(std::ostream& os);

    static ReportResult write_reports(const std::string& path_string, const std::string& snapshot_tag = "");
    static void reset();

};

#endif
