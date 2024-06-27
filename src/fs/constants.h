#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <optional>
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <string>

class Constants {
    public:
    static inline std::optional<std::string> target_path{std::nullopt};
    static inline std::string usage{"cu_fuse <FUSE_PARAMS> -tpath <ABS_TARGET_PATH> <ABS_VIEW_PATH>"};
    static constexpr unsigned int fs_operation_success{0};
    static constexpr unsigned int fs_operation_error{1};
    static inline std::optional<fuse_fill_dir_flags> fill_dir_plus = std::nullopt;

    static constexpr unsigned int ioctl_clear_cache{100};

    static constexpr unsigned int ioctl_log_cache_tables{200};
    static constexpr unsigned int ioctl_log_operation{300};
    static constexpr unsigned int ioctl_log_operation_time{400};
    static constexpr unsigned int ioctl_log_operation_cache_hit{500};
    static constexpr unsigned int ioctl_log_operation_cache_miss{600};

    static constexpr unsigned int ioctl_clear_operation{700};
    static constexpr unsigned int ioctl_clear_operation_cache_hit{800};
    static constexpr unsigned int ioctl_clear_operation_cache_miss{900};
    static constexpr unsigned int ioctl_clear_operation_time{1000};

    static constexpr unsigned int ioctl_log_data_cache_event{1100};
    static constexpr unsigned int ioctl_log_dir_cache_event{1200};
    static constexpr unsigned int ioctl_log_md_cache_event{1300};

    static constexpr unsigned int ioctl_clear_data_cache_event{1400};
    static constexpr unsigned int ioctl_clear_tree_cache_event{1500};
    static constexpr unsigned int ioctl_clear_md_cache_event{1600};

    static constexpr unsigned int ioctl_log_operation_neg{1700};
    static constexpr unsigned int ioctl_clear_operation_neg{1800};

    static constexpr unsigned int ioctl_reset_fs{1900};
    static constexpr unsigned int ioctl_log_all_metrics{2000};

    static inline std::string log_cache_tables_output_filename = "cache_tables.output";
    static inline std::string log_operation_output_filename = "operation_count.output";
    static inline std::string log_operation_time_output_filename = "operation_time.output";
    static inline std::string log_cache_hit_output_filename = "operation_cache_hit.output";
    static inline std::string log_cache_miss_output_filename = "operation_cache_miss.output";
    static inline std::string log_data_cache_event_output_filename = "data_table_cache_event.output";
    static inline std::string log_tree_cache_event_output_filename = "tree_table_cache_event.output";
    static inline std::string log_md_cache_event_output_filename = "md_table_cache_event.output";
    static inline std::string log_operation_cache_neg_output_filename = "operation_cache_neg.output";
};

#endif // CONSTANTS_H
