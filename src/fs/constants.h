#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <optional>
#define FUSE_USE_VERSION 31
#include <fuse.h>

class Constants {
    public:
    static inline std::optional<std::string> target_path{std::nullopt};
    static constexpr unsigned int max_args{10};
    static inline std::string usage{"cu_fuse <FUSE_PARAMS> -tpath <ABS_TARGET_PATH> <ABS_VIEW_PATH>"};
    static constexpr unsigned int fs_operation_success{0};
    static inline std::optional<fuse_fill_dir_flags> fill_dir_plus = std::nullopt;
    static constexpr unsigned int max_print_cache_table_entries = 3;
    static constexpr unsigned int ioctl_clear_cache{100};
    static constexpr unsigned int ioctl_log_cache{200};
    static constexpr unsigned int ioctl_log_operation{300};
    static constexpr unsigned int ioctl_log_operation_time{400};
    static constexpr unsigned int ioctl_log_operation_cache_hit{500};
    static constexpr unsigned int ioctl_log_operation_cache_miss{600};
    static constexpr unsigned int ioctl_clear_operation{700};
    static constexpr unsigned int ioctl_clear_operation_cache_hit{800};
    static constexpr unsigned int ioctl_clear_operation_cache_miss{900};
    static constexpr unsigned int ioctl_clear_operation_time{1000};
    static inline std::string log_cache_output_filename = "log_cache.output";
    static inline std::string log_operation_output_filename = "log_cache_operation.output";
    static inline std::string log_operation_time_output_filename = "log_cache_operation_time.output";
    static inline std::string log_cache_hit_output_filename = "log_cache_hit.output";
    static inline std::string log_cache_miss_output_filename = "log_cache_miss.output";
};

#endif // CONSTANTS_H
