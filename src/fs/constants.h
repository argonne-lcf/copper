#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <optional>
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <string>
#include <unistd.h>

class Constants {
    public:
    static inline std::string target_path{"/"};
    static inline std::optional<std::string> view_path{std::nullopt};
    static inline int log_level{0};
    // FIXME: make struct with strings for types
    static inline std::string log_type{"file_and_stdout"};
    static inline std::optional<std::string> log_output_dir{std::nullopt};
    static inline std::string my_hostname;
    static inline int es{1};
    static inline unsigned long int max_cacheable_byte_size{1048576};
    static inline int md_enoent_ttl_ms{100};
    static inline bool profile_metrics{false};
    static inline int profile_top_n{0};
    static inline bool profile_paths_full{false};
    static inline int profile_snapshot_interval_s{0};
    static inline int address_write_sync_time{5};
    static inline std::optional<std::string> job_nodefile{std::nullopt};
    static inline std::string network_type{"cxi"};
    static inline int trees{1};
    static inline bool prefiltered_address_book{false};

    // clang-format off
    static inline std::string usage{"cu_fuse <FUSE_PARAMS> -tpath <ABS_TARGET_PATH> "
                                                            "-vpath <ABS_VIEW_PATH> "
                                                            "-log_level <0-5> "
                                                            "-log_type <file/stdout/file_and_stdout> "
                                                            "-log_output_dir <ABS_LOG_OUTPUT_DIR_PATH>"
                                                            "-es <NUM_EXECUTION_STREAMS>"
                                                            "-max_cacheable_byte_size <MAX_CACHEABLE_BYTE_SIZE>"
                                                            "-md_enoent_ttl_ms <MD_ENOENT_TTL_MS>"
                                                            "-profile_metrics "
                                                            "-profile_top_n <PROFILE_TOP_N> "
                                                            "-profile_paths_full "
                                                            "-profile_snapshot_interval_s <PROFILE_SNAPSHOT_INTERVAL_S> "
							                                "-nf <PBS_NODEFILE>"
                                                            "-facility_address_book <facility_address_book>"
                                                            "-prefiltered_address_book <0/1>"
                                                            "-trees <num_trees>"
                                                            "-net_type <cxi/na+sm/tcp>"
                                                            "-addr_write_sync_time <time sec>"
                                                            "-s <CU_FUSE_MNT_VIEWDIR>"};
    // clang-format on

    static constexpr unsigned int fs_operation_success{0};
    static constexpr unsigned int fs_operation_error{1};
    static inline std::optional<fuse_fill_dir_flags> fill_dir_plus = std::nullopt;

    static constexpr unsigned int ioctl_clear_cache_tables{100};

    static constexpr unsigned int ioctl_log_cache_tables{200};
    static constexpr unsigned int ioctl_log_operation_count{300};
    static constexpr unsigned int ioctl_log_operation_time{400};
    static constexpr unsigned int ioctl_log_operation_cache_hit{500};
    static constexpr unsigned int ioctl_log_operation_cache_miss{600};

    static constexpr unsigned int ioctl_clear_operation_count{700};
    static constexpr unsigned int ioctl_clear_operation_cache_hit{800};
    static constexpr unsigned int ioctl_clear_operation_cache_miss{900};
    static constexpr unsigned int ioctl_clear_operation_time{1000};

    static constexpr unsigned int ioctl_log_data_cache_event{1100};
    static constexpr unsigned int ioctl_log_tree_cache_event{1200};
    static constexpr unsigned int ioctl_log_md_cache_event{1300};

    static constexpr unsigned int ioctl_clear_data_cache_event{1400};
    static constexpr unsigned int ioctl_clear_tree_cache_event{1500};
    static constexpr unsigned int ioctl_clear_md_cache_event{1600};

    static constexpr unsigned int ioctl_log_operation_neg{1700};
    static constexpr unsigned int ioctl_clear_operation_neg{1800};

    static constexpr unsigned int ioctl_reset_fs{1900};
    static constexpr unsigned int ioctl_log_all_metrics{2000};

    static constexpr unsigned int ioctl_clear_ioctl_event{2100};
    static constexpr unsigned int ioctl_log_ioctl_event{2200};

    static constexpr unsigned int ioctl_get_data_cache_size{2300};
    static constexpr unsigned int ioctl_get_tree_cache_size{2400};
    static constexpr unsigned int ioctl_get_md_cache_size{2500};

    static inline std::string copper_address_book_filename = "copper_address_book.txt";
    static inline std::string copper_address_book_path;
    static inline std::string facility_address_book_path;
    static inline std::string pid;
    static inline std::string logs_subdir = "logs";
    static inline std::string tables_subdir = "tables";
    static inline std::string profiling_subdir = "profiling";
    static inline std::string profiling_final_subdir = "final";
    static inline std::string profiling_cluster_subdir = "cluster";

    static inline std::string output_filename_suffix = "output.log";
    static inline std::string output_filename_path;
    static bool is_valid_user_log_level(const int level) {
        return level >= 0 && level <= 5;
    }
    static int map_user_log_level_to_internal_severity(const int level) {
        switch(level) {
        case 0: return 6; // no logging
        case 1: return 5; // fatal only
        case 2: return 4; // error + fatal
        case 3: return 3; // warning + error + fatal
        case 4: return 2; // info/notice/warning/error/fatal
        case 5: return 1; // debug-heavy / most logging
        default: return 6;
        }
    }
    static bool logging_enabled(const int level) {
        return level > 0;
    }
    static std::string get_output_filename(std::string suffix) {
        return my_hostname + "-" + Constants::pid + "-" + suffix;
    }
    static std::string join_path(const std::string& lhs, const std::string& rhs) {
        if(lhs.empty()) {
            return rhs;
        }
        if(lhs.back() == '/') {
            return lhs + rhs;
        }
        return lhs + "/" + rhs;
    }
    static std::string logs_dir() {
        return join_path(log_output_dir.value(), logs_subdir);
    }
    static std::string tables_dir() {
        return join_path(log_output_dir.value(), tables_subdir);
    }
    static std::string profiling_root_dir() {
        return join_path(log_output_dir.value(), profiling_subdir);
    }
    static std::string profiling_final_dir() {
        return join_path(profiling_root_dir(), profiling_final_subdir);
    }
    static std::string profiling_snapshot_dir(const std::string& snapshot_tag) {
        if(snapshot_tag.empty()) {
            return profiling_final_dir();
        }
        return join_path(profiling_root_dir(), snapshot_tag);
    }
    static std::string profiling_cluster_dir() {
        return join_path(profiling_root_dir(), profiling_cluster_subdir);
    }

    static inline std::string log_cache_tables_output_filename = "cache_tables.output";
    static inline std::string log_operation_output_filename = "operation_count.output";
    static inline std::string log_operation_time_output_filename = "operation_time.output";
    static inline std::string log_cache_hit_output_filename = "operation_cache_hit.output";
    static inline std::string log_cache_miss_output_filename = "operation_cache_miss.output";
    static inline std::string log_data_cache_event_output_filename = "data_table_cache_event.output";
    static inline std::string log_tree_cache_event_output_filename = "tree_table_cache_event.output";
    static inline std::string log_md_cache_event_output_filename = "md_table_cache_event.output";
    static inline std::string log_operation_cache_neg_output_filename = "operation_cache_neg.output";
    static inline std::string log_ioctl_cache_event_output_filename = "ioctl_cache_event.output";
    static inline std::string log_data_cache_size_output_filename = "data_cache_size.output";
    static inline std::string log_tree_cache_size_output_filename = "tree_cache_size.output";
    static inline std::string log_md_cache_size_output_filename = "md_cache_size.output";
    static inline std::string log_md_ttl_event_output_filename = "md_ttl_cache_event.output";
    static inline std::string log_profile_summary_output_filename = "profiling_summary.md";
    static inline std::string log_profile_operations_output_filename = "profiling_operations.csv";
    static inline std::string log_profile_top_paths_output_filename = "profiling_top_paths.csv";
    static inline std::string log_profile_aggregate_output_filename = "profiling_aggregate.csv";
};

#endif // CONSTANTS_H
