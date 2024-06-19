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
    static constexpr unsigned int ioctl_log_operations{300};
    static constexpr unsigned int ioctl_log_operations_time{400};
};

#endif // CONSTANTS_H
