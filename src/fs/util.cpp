#include "util.h"

#include "../metric/ioctl_event.h"
#include "../metric/metrics.h"

std::string Util::rel_to_abs_path(const char* path) {
    if(!path) {
        LOG(FATAL) << "path was null" << std::endl;
        throw std::runtime_error("path was null");
    }

    if(strcmp(path, "/") == 0) {
        return Constants::target_path;
    }

    if(Constants::target_path == "/") {
        return std::string{path};
    }

    const auto suffix{path};
    const auto abs_path{Constants::target_path + suffix};

    return abs_path;
}

std::vector<std::string> Util::process_args(const int argc, char* argv[]) {
    umask(0);

    auto original_string_args{std::vector<std::string>()};
    for(int i{0}; i < argc; i++) {
        std::string cur_string{argv[i]};
        original_string_args.push_back(cur_string);
    }

    auto new_string_args{std::vector<std::string>()};
    for(int i{0}; i < original_string_args.size();) {
        if(original_string_args[i] == "--plus") {
            Constants::fill_dir_plus = FUSE_FILL_DIR_PLUS;
        } else if(original_string_args[i] == "-tpath") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -tpath");
            }

            Constants::target_path = std::string(argv[i + 1]);
            LOG(DEBUG) << "-tpath was found: " << Constants::target_path << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-trees") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -trees");
            }

            Constants::trees = atoi(argv[i + 1]);
            LOG(DEBUG) << "-trees was found: " << Constants::trees << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-vpath") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -vpath");
            }

            Constants::view_path = std::string(argv[i + 1]);
            LOG(DEBUG) << "-vpath was found: " << Constants::view_path.value() << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-log_level") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -log_level");
            }

            // NOTE: aixlog starts at 0 so - 1
            Constants::log_level = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-log_level was found: " << Constants::log_level << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-es") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -es");
            }

            Constants::es = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-es was found: " << Constants::es << std::endl;
            i += 2;
        } else if (original_string_args[i] == "-nf") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -nf");
            }

	        Constants::nodefile = std::string(argv[i+1]);
            LOG(DEBUG) << "-nf was found: " << Constants::nodefile.value() << std::endl;
            i += 2;
	    } else if(original_string_args[i] == "-max_cacheable_byte_size") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -max_cacheable_byte_size");
            }

            Constants::max_cacheable_byte_size = std::stoul(std::string(argv[i + 1]));
            LOG(DEBUG) << "-max_cacheable_byte_size was found: " << Constants::max_cacheable_byte_size << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-net_type") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -net_type");
            }

            Constants::network_type = std::string(argv[i + 1]);
            LOG(DEBUG) << "-net_type was found: " << Constants::network_type << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-addr_write_sync_time") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -addr_write_sync_time");
            }

            Constants::address_write_sync_time = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-addr_write_sync_time was found: " << Constants::address_write_sync_time << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-log_type") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -log_type");
            }

            Constants::log_type = std::string(argv[i + 1]);
            LOG(DEBUG) << "-log_type was found: " << Constants::log_type << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-log_output_dir") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -log_output_dir");
            }

            Constants::log_output_dir = std::string(argv[i + 1]);
            LOG(DEBUG) << "-log_output_dir was found: " << Constants::log_output_dir.value() << std::endl;
            i += 2;
        } else {
            new_string_args.push_back(original_string_args[i]);
            i++;
        }
    }

    if(!Constants::view_path.has_value()) {
        LOG(FATAL) << Constants::usage << std::endl;
        throw std::runtime_error("-vpath argument not found");
    }

    return new_string_args;
}

std::vector<std::byte> Util::read_ent_file(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
	LOG(WARNING) << "read file failed with errno: " << std::strerror(errno) << std::endl; 
        throw std::system_error(errno, std::generic_category(), "failed to open file: " + path);
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
	LOG(WARNING) << "read file failed with errno: " << std::strerror(errno) << std::endl; 
        throw std::system_error(errno, std::generic_category(), "failed to get file size for: " + path);
    }

    std::vector<std::byte> bytes(file_stat.st_size);

    ssize_t bytes_read = read(fd, bytes.data(), bytes.size());
    if (bytes_read == -1) {
        close(fd);
	LOG(WARNING) << "read file failed with errno: " << std::strerror(errno) << std::endl; 
        throw std::system_error(errno, std::generic_category(), "failed to read file: " + path);
    }

    close(fd);

    return bytes;
}

std::optional<std::ofstream> Util::try_get_fstream_from_path(const char* path) {
    if(path == nullptr) {
        LOG(ERROR) << "cstr path was null" << std::endl;
        return std::nullopt;
    }

    const std::string output_path_string = std::string(path);
    std::ofstream file(output_path_string, std::ios::out | std::ios::trunc);

    if(!file.is_open()) {
        LOG(ERROR) << "failed to open path: " << output_path_string << std::endl;
        return std::nullopt;
    }

    LOG(DEBUG) << "succesfully opened path: " << output_path_string << std::endl;

    return file;
}

#define GET_FS_STREAM(path_string, filename)                                     \
    output = std::filesystem::path(path_string).parent_path() += "/" + filename; \
    fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());             \
    if(!fs_stream_opt.has_value()) {                                             \
        LOG(ERROR) << "failed to open fstream" << std::endl;                     \
        return;                                                                  \
    }

void Util::log_all_metrics(const std::string& path_string) {
    std::string output;
    std::optional<std::ofstream> fs_stream_opt = std::nullopt;

    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_cache_tables_output_filename));
    fs_stream_opt.value() << CacheTables::tree_cache_table << std::endl;
    fs_stream_opt.value() << CacheTables::data_cache_table << std::endl;
    fs_stream_opt.value() << CacheTables::md_cache_table << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_operation_output_filename));
    fs_stream_opt.value() << Operations::log_operation << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_operation_time_output_filename));
    fs_stream_opt.value() << Operations::log_operation_time << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_cache_hit_output_filename));
    fs_stream_opt.value() << Operations::log_operation_cache_hit << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_cache_miss_output_filename));
    fs_stream_opt.value() << Operations::log_operation_cache_miss << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_data_cache_event_output_filename));
    fs_stream_opt.value() << CacheEvent::log_data_cache_event << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_tree_cache_event_output_filename));
    fs_stream_opt.value() << CacheEvent::log_tree_cache_event << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_md_cache_event_output_filename));
    fs_stream_opt.value() << CacheEvent::log_md_cache_event << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_operation_cache_neg_output_filename));
    fs_stream_opt.value() << Operations::log_operation_neg << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_ioctl_cache_event_output_filename));
    fs_stream_opt.value() << IoctlEvent::log_ioctl_event << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_data_cache_size_output_filename));
    fs_stream_opt.value() << DataCacheTable::get_data_size_metrics << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_tree_cache_size_output_filename));
    fs_stream_opt.value() << TreeCacheTable::get_data_size_metrics << std::endl;
    GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_md_cache_size_output_filename));
    fs_stream_opt.value() << MDCacheTable::get_data_size_metrics << std::endl;
}

void Util::reset_fs() {
    CacheTables::data_cache_table.cache.clear();
    CacheTables::md_cache_table.cache.clear();
    CacheTables::tree_cache_table.cache.clear();

    CacheEvent::reset_data_cache_event();
    CacheEvent::reset_md_cache_event();
    CacheEvent::reset_tree_cache_event();

    Operations::reset_operation_cache_hit();
    Operations::reset_operation_cache_miss();
    Operations::reset_operation_counter();
    Operations::reset_operation_cache_neg();
    Operations::reset_operation_timer();

    IoctlEvent::reset_ioctl_event();
}

bool Util::is_recursive_path_string(const std::string& path_string) {
    if(!Constants::view_path.has_value()) {
        throw std::runtime_error("view_path was std::nullopt!");
    }

    const auto& view_path_value = Constants::view_path.value();
    auto res = std::mismatch(view_path_value.begin(), view_path_value.end(), path_string.begin());

    return res.first == view_path_value.end();
}
