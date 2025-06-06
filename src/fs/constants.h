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
    static inline std::string net_type{"cxi"};
    static inline std::optional<std::string> log_output_dir{std::nullopt};
    static inline std::string my_hostname;
    static inline int es{1};
    static inline unsigned long int max_cacheable_byte_size{1048576};
    static inline int address_write_sync_time{5};
    static inline std::optional<std::string> job_nodefile{std::nullopt};
    static inline int trees{1};

    // clang-format off
    static inline std::string usage{"cu_fuse <FUSE_PARAMS> -tpath <ABS_TARGET_PATH> "
                                                            "-vpath <ABS_VIEW_PATH> "
                                                            "-log_level <0-6> "
                                                            "-log_type <file/stdout/file_and_stdout> "
                                                            "-log_output_dir <ABS_LOG_OUTPUT_DIR_PATH>"
                                                            "-es <NUM_EXECUTION_STREAMS>"
                                                            "-max_cacheable_byte_size <MAX_CACHEABLE_BYTE_SIZE>"
							                                "-nf <PBS_NODEFILE>"
                                                            "-facility_address_book <facility_address_book>"
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

    static inline std::string output_filename_suffix = "output.log";
    static inline std::string output_filename_path;
    static std::string get_output_filename(std::string suffix) {
        return my_hostname + "-" + Constants::pid + "-" + suffix;
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
};

#endif // CONSTANTS_H
