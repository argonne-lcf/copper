#include "util.h"

#include <filesystem>
#include <sstream>
#include <mutex>
#include "../startup/daemon_identity.h"

#include "../metric/ioctl_event.h"
#include "../metric/metrics.h"
#include "../metric/profiling.h"
#include "../startup/startup_tools.h"

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

            Constants::log_level = std::stoi(std::string(argv[i + 1]));
            if(!Constants::is_valid_user_log_level(Constants::log_level)) {
                throw std::runtime_error("-log_level must be in range 0-5");
            }
            Constants::startup_log_level = Constants::log_level;
            Constants::operation_log_level = Constants::log_level;
            LOG(DEBUG) << "-log_level was found: " << Constants::log_level << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-startup_log_level") {
            if(i + 1 >= original_string_args.size()) throw std::runtime_error("no argument after -startup_log_level");
            Constants::startup_log_level = CopperStartup::parse_log_level(argv[i + 1], "startup");
            i += 2;
        } else if(original_string_args[i] == "-operation_log_level") {
            if(i + 1 >= original_string_args.size()) throw std::runtime_error("no argument after -operation_log_level");
            Constants::operation_log_level = CopperStartup::parse_log_level(argv[i + 1], "operation");
            i += 2;
        } else if(original_string_args[i] == "-engine_startup_timeout_ms") {
            if(i + 1 >= original_string_args.size()) throw std::runtime_error("no argument after -engine_startup_timeout_ms");
            Constants::engine_startup_timeout_ms = CopperStartup::parse_positive_timeout_ms(argv[i + 1], "engine startup");
            i += 2;
        } else if(original_string_args[i] == "-parent_ready_timeout_ms") {
            if(i + 1 >= original_string_args.size()) throw std::runtime_error("no argument after -parent_ready_timeout_ms");
            Constants::parent_ready_timeout_ms = CopperStartup::parse_positive_timeout_ms(argv[i + 1], "parent readiness");
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

	        Constants::job_nodefile = std::string(argv[i+1]);
            LOG(DEBUG) << "-nf was found: " << Constants::job_nodefile.value() << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-facility_address_book") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -facility_address_book");
            }

            Constants::facility_address_book_path = std::string(argv[i+1]);
            LOG(DEBUG) << "-facility_address_book was found: " << Constants::facility_address_book_path << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-prefiltered_address_book") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -prefiltered_address_book");
            }

            Constants::prefiltered_address_book = std::stoi(std::string(argv[i + 1])) != 0;
            LOG(DEBUG) << "-prefiltered_address_book was found: "
                       << Constants::prefiltered_address_book << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-max_cacheable_byte_size") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -max_cacheable_byte_size");
            }

            Constants::max_cacheable_byte_size = std::stoul(std::string(argv[i + 1]));
            LOG(DEBUG) << "-max_cacheable_byte_size was found: " << Constants::max_cacheable_byte_size << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-md_enoent_ttl_ms") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -md_enoent_ttl_ms");
            }

            Constants::md_enoent_ttl_ms = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-md_enoent_ttl_ms was found: " << Constants::md_enoent_ttl_ms << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-profile_metrics") {
            Constants::profile_metrics = true;
            LOG(DEBUG) << "-profile_metrics was found" << std::endl;
            i += 1;
        } else if(original_string_args[i] == "-profile_top_n") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -profile_top_n");
            }

            Constants::profile_metrics = true;
            Constants::profile_top_n = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-profile_top_n was found: " << Constants::profile_top_n << std::endl;
            i += 2;
        } else if(original_string_args[i] == "-profile_paths_full") {
            Constants::profile_metrics = true;
            Constants::profile_paths_full = true;
            LOG(DEBUG) << "-profile_paths_full was found" << std::endl;
            i += 1;
        } else if(original_string_args[i] == "-profile_snapshot_interval_s") {
            if(i + 1 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -profile_snapshot_interval_s");
            }

            Constants::profile_metrics = true;
            Constants::profile_snapshot_interval_s = std::stoi(std::string(argv[i + 1]));
            LOG(DEBUG) << "-profile_snapshot_interval_s was found: " << Constants::profile_snapshot_interval_s << std::endl;
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
        errno = EINVAL;
        LOG(ERROR) << "cstr path was null" << std::endl;
        return std::nullopt;
    }

    const std::string output_path_string = std::string(path);
    std::ofstream file(output_path_string, std::ios::out | std::ios::trunc);

    if(!file.is_open()) {
        const int saved_errno = errno;
        LOG(ERROR) << "failed to open path: " << output_path_string << std::endl;
        errno = saved_errno;
        return std::nullopt;
    }

    LOG(DEBUG) << "succesfully opened path: " << output_path_string << std::endl;

    return file;
}

#define GET_FS_STREAM(path_string, filename)                                         \
    if(fs_stream_opt) { fs_stream_opt->flush(); fs_stream_opt->close(); if(!*fs_stream_opt) throw std::runtime_error("metric report write failed"); } \
    output = (std::filesystem::path(Constants::tables_dir()) / Constants::profiling_final_subdir / filename).string(); \
    fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());             \
    if(!fs_stream_opt.has_value()) {                                             \
        LOG(ERROR) << "failed to open fstream" << std::endl;                     \
        throw std::runtime_error("metric report open failed");                     \
    }

ReportResult Util::log_all_metrics(const std::string& path_string) {
    try {
    std::string output;
    std::optional<std::ofstream> fs_stream_opt = std::nullopt;
    std::filesystem::create_directories(std::filesystem::path(Constants::tables_dir()) / Constants::profiling_final_subdir);

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

    if(Profiling::enabled()) {
        GET_FS_STREAM(path_string, Constants::get_output_filename(Constants::log_md_ttl_event_output_filename));
        fs_stream_opt.value() << Profiling::log_md_ttl_event << std::endl;
    }
    fs_stream_opt->flush(); fs_stream_opt->close();
    if(!*fs_stream_opt) throw std::runtime_error("metric report write failed");
    return publish_cache_snapshot();
    } catch(const std::exception& error) { ReportResult result; result.error = error.what(); return result; }
}

namespace {
std::mutex cache_publication_mutex;
uint64_t cache_sequence = 0;
uint64_t cache_epoch = 0;
void durable_cache_file(const std::filesystem::path& path, const std::string& bytes) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if(fd < 0) throw std::system_error(errno, std::generic_category(), "create cache payload");
    struct Guard { int fd; ~Guard() { close(fd); } } guard{fd};
    size_t offset = 0;
    while(offset < bytes.size()) {
        auto count = write(fd, bytes.data() + offset, bytes.size() - offset);
        if(count < 0 && errno == EINTR) continue;
        if(count <= 0) throw std::system_error(errno ? errno : EIO, std::generic_category(), "write cache payload");
        offset += count;
    }
    if(fsync(fd) != 0) throw std::system_error(errno, std::generic_category(), "fsync cache payload");
}
void durable_cache_directory(const std::filesystem::path& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if(fd < 0) throw std::system_error(errno, std::generic_category(), "open cache directory");
    const int status = fsync(fd), saved = errno; close(fd);
    if(status != 0) throw std::system_error(saved, std::generic_category(), "fsync cache directory");
}
}

ReportResult Util::publish_cache_snapshot() {
    std::lock_guard<std::mutex> guard(cache_publication_mutex);
    ReportResult result;
    std::filesystem::path staging;
    try {
        const char* run = std::getenv("COPPER_RUN_ID");
        const char* generation = std::getenv("COPPER_DAEMON_GENERATION");
        if(!run || !*run || !generation || !*generation) throw std::runtime_error("cache snapshot requires COPPER_RUN_ID and COPPER_DAEMON_GENERATION");
        const auto root = std::filesystem::path(Constants::tables_dir()) / "cache-snapshots";
        std::filesystem::create_directories(root);
        std::string pattern = (root / ".pending-XXXXXXXX").string();
        std::vector<char> temporary(pattern.begin(), pattern.end()); temporary.push_back('\0');
        const char* created = mkdtemp(temporary.data());
        if(!created) throw std::system_error(errno, std::generic_category(), "create cache generation");
        staging = created;
        const auto sequence = ++cache_sequence;
        // Each formatter takes its table's own lock/copy. This is per-table
        // occupancy capture, NOT a simultaneous multi-cache or cluster snapshot.
        std::ostringstream data, tree, metadata;
        DataCacheTable::get_data_size_metrics(data);
        TreeCacheTable::get_data_size_metrics(tree);
        MDCacheTable::get_data_size_metrics(metadata);
        const std::vector<std::pair<std::string, std::string>> tables = {
            {"data", data.str()}, {"tree", tree.str()}, {"metadata", metadata.str()}};
        std::ostringstream manifest;
        manifest << "{\"schema_version\":1,\"run_id\":" << CopperStartup::identity_json_string(run)
                 << ",\"daemon_generation\":" << CopperStartup::identity_json_string(generation)
                 << ",\"host\":" << CopperStartup::identity_json_string(Constants::my_hostname)
                 << ",\"epoch\":" << cache_epoch << ",\"sequence\":" << sequence
                 << ",\"capture_semantics\":\"per-table-locked-copy\",\"tables\":{";
        bool first = true;
        for(const auto& table : tables) {
            const auto filename = table.first + ".output";
            durable_cache_file(staging / filename, table.second);
            if(!first) manifest << ',';
            first = false;
            manifest << CopperStartup::identity_json_string(table.first) << ":{\"path\":"
                     << CopperStartup::identity_json_string(filename) << ",\"sha256\":"
                     << CopperStartup::identity_json_string(profiler_sha256(table.second)) << '}';
        }
        manifest << "}}\n";
        durable_cache_file(staging / "cache_snapshot.json.pending", manifest.str());
        durable_cache_directory(staging);
        const auto final = root / ("snapshot-" + staging.filename().string().substr(9));
        std::filesystem::rename(staging, final);
        staging.clear();
        // The consumer recursively searches for cache_snapshot.json; keep the
        // commit name invisible even while it traverses a staging directory.
        std::filesystem::rename(final / "cache_snapshot.json.pending", final / "cache_snapshot.json");
        durable_cache_directory(final);
        durable_cache_directory(root);
        result.success = true; result.manifest_path = (final / "cache_snapshot.json").string();
        for(const auto& table : tables) result.files.push_back((final / (table.first + ".output")).string());
    } catch(const std::exception& error) {
        result.error = error.what();
        if(!staging.empty()) { std::error_code ignored; std::filesystem::remove_all(staging, ignored); }
    }
    return result;
}

ReportResult Util::log_profiling_metrics(const std::string& path_string, const std::string& snapshot_tag) {
    ReportResult cache; cache.success = true;
    if(snapshot_tag.empty()) { cache = publish_cache_snapshot(); if(!cache.success) return cache; }
    if(!Profiling::enabled()) return cache;
    return Profiling::write_reports(path_string, snapshot_tag);
}

ReportResult Util::reset_fs() {
    CacheTables::data_cache_table.clear();
    CacheTables::md_cache_table.clear();
    CacheTables::tree_cache_table.clear();


    IoctlEvent::reset_ioctl_event();
    const auto report = Profiling::reset();
    if(!report.success) {
        LOG(ERROR) << "failed to publish closing profiling interval: " << report.error << std::endl;
    }
    return report;
}

bool Util::is_recursive_path_string(const std::string& path_string) {
    if(!Constants::view_path.has_value()) {
        throw std::runtime_error("view_path was std::nullopt!");
    }

    const auto& view_path_value = Constants::view_path.value();
    auto res = std::mismatch(view_path_value.begin(), view_path_value.end(), path_string.begin());

    return res.first == view_path_value.end();
}
