#include "cu_fuse.h"
#include "error_status.h"
#include "fuse_policy.h"
#include "../metric/profiling.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <condition_variable>

namespace tl = thallium;

#define CU_FUSE_RPC_DATA "cu_fuse_rpc_data"
#define CU_FUSE_RPC_METADATA "cu_fuse_rpc_metadata"

namespace {
std::atomic<bool> g_profile_snapshot_thread_stop{false};
std::thread g_profile_snapshot_thread;
std::mutex g_profile_snapshot_mutex;
std::condition_variable g_profile_snapshot_cv;

void stop_profile_snapshot_thread() {
    g_profile_snapshot_thread_stop.store(true);
    g_profile_snapshot_cv.notify_all();
    if(g_profile_snapshot_thread.joinable()) {
        g_profile_snapshot_thread.join();
    }
}

void start_profile_snapshot_thread() {
    if(!Profiling::enabled() || Constants::profile_snapshot_interval_s <= 0) {
        return;
    }

    g_profile_snapshot_thread_stop.store(false);
    g_profile_snapshot_thread = std::thread([] {
        std::unique_lock<std::mutex> lock(g_profile_snapshot_mutex);
        while(!g_profile_snapshot_cv.wait_for(lock, std::chrono::seconds(Constants::profile_snapshot_interval_s),
                                              [] { return g_profile_snapshot_thread_stop.load(); })) {
            lock.unlock();
            Util::log_profiling_metrics(Constants::output_filename_path, "runtime-snapshot");
            lock.lock();
        }
    });
}
} // namespace

static int cu_fuse_getattr(const char* path_, struct stat* stbuf, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::getattr, path_);
    if(Util::is_recursive_path_string(path_string)) {
        LOG(WARNING) << "path_string was recursive: " << path_string << std::endl;
        return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::neg,
        ProfileSource::metadata, path_string, start, -ENOENT);
    }

    auto cu_stat_opt{CacheTables::md_cache_table.get(path_string)};
    OperationResult result = OperationResult::cache_hit;
    if(!cu_stat_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(DEBUG, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " from client running at address " << engine->self() << std::endl;
        LOG(DEBUG, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::lstat_final_return_type rpc_lstat_response =
                             ServerLocalCacheProvider::rpc_lstat.on(engine->self())(true, path_string));

        if(rpc_lstat_response != 0) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::neg,
            ProfileSource::metadata, path_string, start, rpc_lstat_response);
        }

        result = OperationResult::cache_miss;
        // Re-read below so a failed/absent cache population is returned as EIO.
    }

    cu_stat_opt = CacheTables::md_cache_table.get(path_string);
    if(!cu_stat_opt.has_value()) {
        LOG(ERROR) << "metadata cache missing after successful rpc" << std::endl;
        return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::neg,
        ProfileSource::metadata, path_string, start, -EIO);
    }
    const auto cu_stat = cu_stat_opt.value();
    cu_stat.cp_to_buf(stbuf);

    return Metric::stop_cache_operation(OperationFunction::getattr, result,
    ProfileSource::metadata, path_string, start, Constants::fs_operation_success);
}

static int cu_fuse_read(const char* path_, char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::read, path_);
    const int offset_error = FusePolicy::read_offset_result(offset);
    if(offset_error != 0) {
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
        ProfileSource::data, path_string, start, offset_error);
    }
    if(Util::is_recursive_path_string(path_string)) {
        LOG(WARNING) << "path_string was recursive: " << path_string << std::endl;
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
        ProfileSource::data, path_string, start, -ENOENT);
    }

    LOG(DEBUG) << "requested offset: " << offset << std::endl;
    LOG(DEBUG) << "requested size: " << size << std::endl;

    auto md_entry = CacheTables::md_cache_table.get(path_string);
    if(md_entry.has_value()) {
        struct stat* md_st = (struct stat*)md_entry->get_vec().data();
        if(md_st->st_size >= Constants::max_cacheable_byte_size) {
            LOG(DEBUG) << "file larger than max cacheable size... going to lustre" << std::endl;

            int fd = open(path_string.c_str(), O_RDONLY | O_CLOEXEC);
            if(fd == -1) {
                return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
                ProfileSource::data, path_string, start, -errno);
            }
            ssize_t res = pread(fd, buf, size, offset);
            const int saved_errno = errno;
            const int close_result = close(fd);
            if(res == -1) {
                res = -saved_errno;
            } else if(close_result == -1) {
                res = -errno;
            }

            return Metric::stop_cache_operation(OperationFunction::read,
            res < 0 ? OperationResult::neg : OperationResult::success,
            ProfileSource::bypass, path_string, start, res);
        }
    }

    const auto entry_opt = CacheTables::data_cache_table.get(path_string);
    std::vector<std::byte> bytes;
    bool cache = false;

    if(!entry_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(DEBUG, CU_FUSE_RPC_DATA) << __FUNCTION__ << " from client running at address: " << engine->self() << std::endl;
        LOG(DEBUG, CU_FUSE_RPC_DATA) << __FUNCTION__ << " cu_fuse_read thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::read_final_return_type rpc_readfile_response =
                             ServerLocalCacheProvider::rpc_readfile.on(engine->self())(true, path_string));

        if(rpc_readfile_response < 0) {
            LOG(WARNING) << "failed to passthrough readfile" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
            ProfileSource::data, path_string, start, rpc_readfile_response);
        }

        const auto populated = CacheTables::data_cache_table.get(path_string);
        if(!populated.has_value()) {
            LOG(ERROR) << "data cache missing after successful rpc" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
            ProfileSource::data, path_string, start, -EIO);
        }
        bytes = std::move(populated.value());
        cache = true;
    } else {
        bytes = std::move(entry_opt.value());
    }

    size_t read_size = 0;
    if(offset < static_cast<off_t>(bytes.size())) {
        // Calculate the amount to copy, ensuring not to exceed the bounds of the vector
        ssize_t copy_size = std::min(static_cast<size_t>(size), bytes.size() - static_cast<size_t>(offset));

        // Copy data from bytes to buf
        std::memcpy(buf, bytes.data() + offset, copy_size);
        read_size = static_cast<size_t>(copy_size);
    }

    if(cache) {
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::cache_miss,
        ProfileSource::data, path_string, start, read_size);
    } else {
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::cache_hit,
        ProfileSource::data, path_string, start, read_size);
    }
}

static int
cu_fuse_readdir(const char* path_, void* buf, const fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::readdir, path_);
    if(Util::is_recursive_path_string(path_string)) {
        LOG(WARNING) << "path_string was recursive: " << path_string << std::endl;
        return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::neg,
        ProfileSource::tree, path_string, start, -ENOENT);
    }

    const auto& tree_cache_table_entry_opt = CacheTables::tree_cache_table.get(path_string);
    std::vector<std::string> entries{};
    bool cache = false;

    if(!tree_cache_table_entry_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(DEBUG, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " from client running at address " << engine->self() << std::endl;
        LOG(DEBUG, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::readdir_final_return_type rpc_readdir_response =
                             ServerLocalCacheProvider::rpc_readdir.on(engine->self())(true, path_string));

        if(rpc_readdir_response != Constants::fs_operation_success) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::neg,
            ProfileSource::tree, path_string, start, rpc_readdir_response);
        }

        const auto populated = CacheTables::tree_cache_table.get(path_string);
        if(!populated.has_value()) {
            LOG(ERROR) << "tree cache missing after successful rpc" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::neg,
            ProfileSource::tree, path_string, start, -EIO);
        }
        entries = std::move(populated.value());
        cache = true;
    } else {
        entries = tree_cache_table_entry_opt.value();
    }

    const fuse_fill_dir_flags fill_dir{
    (Constants::fill_dir_plus.has_value()) ? FUSE_FILL_DIR_PLUS : static_cast<fuse_fill_dir_flags>(NULL)};

    for(const auto& cur_path_stem : entries) {
        const auto cur_full_path = std::string(path_string).append("/").append(cur_path_stem);

        if(filler(buf, cur_path_stem.c_str(), nullptr, 0, fill_dir) == 1) {
            break;
        }
    }

    if(cache) {
        return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::cache_miss,
        ProfileSource::tree, path_string, start, Constants::fs_operation_success);
    } else {
        return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::cache_hit,
        ProfileSource::tree, path_string, start, Constants::fs_operation_success);
    }
}

#define IOCTL_GET_FS_STREAM(filename)                                                                    \
    do {                                                                                                 \
        output = std::filesystem::path(path_string).parent_path() += "/" + filename;                    \
        errno = 0;                                                                                       \
        fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());                                 \
        if(!fs_stream_opt.has_value()) {                                                                 \
            const int status = CopperError::negative_errno_or_eio(errno);                                \
            LOG(ERROR) << "failed to open fstream: " << -status << std::endl;                            \
            return Metric::stop_cache_operation(OperationFunction::ioctl, OperationResult::neg,          \
                                                ProfileSource::bypass, path_string, start, status);       \
        }                                                                                                \
    } while(false)

static int cu_fuse_ioctl(const char* path_, int cmd, void* arg, struct fuse_file_info*, unsigned int flags, void* data) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::ioctl, path_);
    if(Util::is_recursive_path_string(path_string)) {
        LOG(WARNING) << "path_string was recursive: " << path_string << std::endl;
        return Metric::stop_cache_operation(OperationFunction::ioctl, OperationResult::neg,
        ProfileSource::bypass, path_string, start, -ENOENT);
    }

    std::string output;
    std::optional<std::ofstream> fs_stream_opt = std::nullopt;

    switch(cmd) {
    case(Constants::ioctl_log_cache_tables):
        LOG(INFO) << "logging cache" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_cache_tables_output_filename));
        fs_stream_opt.value() << CacheTables::tree_cache_table << std::endl;
        fs_stream_opt.value() << CacheTables::data_cache_table << std::endl;
        fs_stream_opt.value() << CacheTables::md_cache_table << std::endl;
        break;
    case(Constants::ioctl_clear_cache_tables):
        LOG(INFO) << "clearing cache" << std::endl;
        CacheTables::tree_cache_table.clear();
        CacheTables::md_cache_table.clear();
        CacheTables::data_cache_table.clear();
        break;
    case(Constants::ioctl_log_operation_count):
        LOG(INFO) << "logging operation" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_operation_output_filename));
        fs_stream_opt.value() << Operations::log_operation << std::endl;
        break;
    case(Constants::ioctl_log_operation_time):
        LOG(INFO) << "logging operation time (ms)" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_operation_time_output_filename));
        fs_stream_opt.value() << Operations::log_operation_time << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_hit):
        LOG(INFO) << "logging operation cache hit" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_cache_hit_output_filename));
        fs_stream_opt.value() << Operations::log_operation_cache_hit << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_miss):
        LOG(INFO) << "logging operation cache miss" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_cache_miss_output_filename));
        fs_stream_opt.value() << Operations::log_operation_cache_miss << std::endl;
        break;
    case(Constants::ioctl_clear_operation_count):
        LOG(INFO) << "clearing operation" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_clear_operation_cache_hit):
        LOG(INFO) << "clearing operation cache hit" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_clear_operation_cache_miss):
        LOG(INFO) << "clearing operation cache miss" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_clear_operation_time):
        LOG(INFO) << "clearing operation time" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_log_data_cache_event):
        LOG(INFO) << "logging data cache event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_data_cache_event_output_filename));
        fs_stream_opt.value() << CacheEvent::log_data_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_tree_cache_event):
        LOG(INFO) << "logging tree cache event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_tree_cache_event_output_filename));
        fs_stream_opt.value() << CacheEvent::log_tree_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_md_cache_event):
        LOG(INFO) << "logging md cache event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_md_cache_event_output_filename));
        fs_stream_opt.value() << CacheEvent::log_md_cache_event << std::endl;
        break;
    case(Constants::ioctl_clear_data_cache_event):
        LOG(INFO) << "clearing data cache event" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_clear_tree_cache_event):
        LOG(INFO) << "clearing tree cache event" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_clear_md_cache_event):
        LOG(INFO) << "clearing md cache event" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_log_operation_neg): {
        LOG(INFO) << "logging operation neg event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_operation_cache_neg_output_filename));
        fs_stream_opt.value() << Operations::log_operation_neg << std::endl;
        break;
    }
    case(Constants::ioctl_clear_operation_neg):
        LOG(INFO) << "clearing operation neg event" << std::endl;
        Profiling::reset();
        break;
    case(Constants::ioctl_log_all_metrics):
        LOG(INFO) << "loggin all metrics" << std::endl;
        Util::log_all_metrics(path_string);
        Util::log_profiling_metrics(path_string, "ioctl-snapshot");
        break;
    case(Constants::ioctl_reset_fs):
        LOG(INFO) << "resetting filesystem" << std::endl;
        Util::log_profiling_metrics(path_string, "pre-reset");
        Util::reset_fs();
        break;
    case(Constants::ioctl_log_ioctl_event):
        LOG(INFO) << "logging ioctl event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_ioctl_cache_event_output_filename));
        fs_stream_opt.value() << IoctlEvent::log_ioctl_event << std::endl;
        break;
    case(Constants::ioctl_clear_ioctl_event):
        LOG(INFO) << "clearing ioctl event" << std::endl;
        IoctlEvent::reset_ioctl_event();
        break;
    case(Constants::ioctl_get_data_cache_size):
        LOG(INFO) << "logging data cache size" << std::endl;
        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_data_cache_size_output_filename));
        fs_stream_opt.value() << DataCacheTable::get_data_size_metrics << std::endl;
        break;
    case(Constants::ioctl_get_tree_cache_size):
        LOG(INFO) << "logging tree cache size" << std::endl;
        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_tree_cache_size_output_filename));
        fs_stream_opt.value() << TreeCacheTable::get_data_size_metrics << std::endl;
        break;
    case(Constants::ioctl_get_md_cache_size):
        LOG(INFO) << "logging md cache size" << std::endl;
        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_md_cache_size_output_filename));
        fs_stream_opt.value() << MDCacheTable::get_data_size_metrics << std::endl;
        break;
    default:
        LOG(DEBUG) << "external ioctl with cmd: " << cmd << std::endl;
        Operations::inc_operation(OperationFunction::ext_getattr);
        IoctlEvent::record_ioctl_event(path_string, cmd);
        break;
    }

    return Metric::stop_operation(OperationFunction::ioctl, start, Constants::fs_operation_success);
}

static void start_thallium_engine() {
    try {
        LOG(INFO) << "starting thallium engine" << std::endl;
        ServerLocalCacheProvider::provider_ready = false;
        ServerLocalCacheProvider::provider_ready_after_us = -1;
        ServerLocalCacheProvider::parent_provider_ready_cached = false;
        ServerLocalCacheProvider::first_successful_parent_rpc_after_us = -1;
        ServerLocalCacheProvider::provider_start_time = std::chrono::steady_clock::now();

        LOG(INFO) << "redirecting stderr to output_file" << std::endl;
        if(freopen(Constants::output_filename_path.c_str(), "a", stderr) == nullptr) {
            LOG(ERROR) << "failed to redirect stderr to path: " << Constants::output_filename_path << std::endl;
        };

        auto logger = new TLLogger();
        tl::logger::set_global_logger(logger);
        tl::logger::set_global_log_level(tl::logger::level::trace);

        tl::engine* server_engine;
        std::string my_addr{};
        try {
            server_engine = new tl::engine{Constants::network_type, THALLIUM_SERVER_MODE, true, Constants::es};
            my_addr = std::string(server_engine->self());
	          LOG(INFO) << "engine started" << std::endl;
        } catch(std::exception& e) {
            LOG(FATAL) << e.what() << std::endl;
            return;
        }

        server_engine->set_logger(logger);
        server_engine->set_log_level(tl::logger::level::debug);

        LOG(INFO) << "using network type: " << Constants::network_type << std::endl;

        if(Constants::job_nodefile.has_value()) {
            LOG(INFO) << "using job_nodefile to init addresses" << std::endl;
            LOG(INFO) << "Generating CXI address from job_nodefile: " << Constants::job_nodefile.value() << std::endl;
            // NodeTree::generate_nodelist_from_nodefile(Constants::job_nodefile.value());
            NodeTree::parse_nodelist_from_facility_address_book();
        } else if(Constants::network_type == "cxi") {
            LOG(INFO) << "parsing cxi network file to init addresses" << std::endl;
            NodeTree::get_hsn0_cxi_addr();

            LOG(INFO) << "address written to address_book... sleeping for synchronization time: " << Constants::address_write_sync_time << std::endl;
            sleep(Constants::address_write_sync_time);

        } else if(Constants::network_type == "na+sm" || Constants::network_type == "tcp") {
            LOG(INFO) << "using address from server engine to init addresses" << std::endl;
            NodeTree::push_back_address(Constants::my_hostname, my_addr);
            LOG(INFO) << "address written to address_book... sleeping for synchronization time: " << Constants::address_write_sync_time << std::endl;
            sleep(Constants::address_write_sync_time);

            NodeTree::parse_nodelist_from_facility_address_book();
        } else {
            LOG(FATAL) << "invalid network type" << std::endl;
            return;
        }

        if(ServerLocalCacheProvider::node_address_data.size() < Constants::trees) {
            LOG(FATAL) << "number of tree segments cannot be greater than number of addresses" << std::endl;
            LOG(FATAL) << "number of addresses: " << ServerLocalCacheProvider::node_address_data.size() << std::endl;
            LOG(FATAL) << "number of tree segments: " << Constants::trees << std::endl;
            return;
        }
        std::vector<std::string> my_tree_segment = NodeTree::get_my_tree_segment(ServerLocalCacheProvider::node_address_data, 
                                                                                 my_addr, Constants::trees);

        LOG(INFO) << "building rpc overlay network tree" << std::endl;
        Node::root = NodeTree::build_my_tree(Node::root, my_tree_segment);

        LOG(INFO) << "setting parent node addr" << std::endl;
        if(Node::root->addr != my_addr) {
            std::string parent{};
            NodeTree::get_parent_from_tree(Node::root, my_addr, parent);
            Node::parent_addr = parent;
            LOG(INFO) << "non-root node setting parent addr: " << Node::parent_addr.value() << std::endl;
        } else {
            LOG(INFO) << "root node not setting parent addr" << std::endl;
        }

        NodeTree::print_tree(Node::root);
        int tree_depth = NodeTree::depth(Node::root);
        LOG(INFO) << "the depth of the tree is: " << tree_depth << std::endl;
        NodeTree::pretty_print_tree(Node::root, tree_depth);

        LOG(INFO) << "server running at address: " << my_addr << std::endl;
        server_engine->enable_remote_shutdown();
        ServerLocalCacheProvider::rpc_lstat = server_engine->define("rpc_lstat");
        ServerLocalCacheProvider::rpc_readfile = server_engine->define("rpc_readfile");
        ServerLocalCacheProvider::rpc_readdir = server_engine->define("rpc_readdir");
        ServerLocalCacheProvider::rpc_is_ready = server_engine->define("rpc_is_ready");
        new ServerLocalCacheProvider{*server_engine, my_tree_segment};

        LOG(INFO) << "setting my_engine" << std::endl;
        ServerLocalCacheProvider::my_engine = server_engine;
        ServerLocalCacheProvider::provider_ready = true;
        ServerLocalCacheProvider::provider_ready_after_us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - ServerLocalCacheProvider::provider_start_time)
                .count();
        LOG(INFO) << "provider registration completed after "
                  << ServerLocalCacheProvider::provider_ready_after_us.load()
                  << " us" << std::endl;

    } catch(const std::exception& e) {
        LOG(FATAL) << "Exception caught in thread: " << e.what() << std::endl;
    }
}

static void* cu_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    auto start = Metric::start_operation(OperationFunction::init);

    // NOTE: docs (https://libfuse.github.io/doxygen/structfuse__config.html#a3e84d36c87733fcafc594b18a6c3dda8)
    cfg->entry_timeout = 0;
    cfg->negative_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->use_ino = false;
    cfg->readdir_ino = false;
    cfg->direct_io = true;
    cfg->kernel_cache = true;
    cfg->auto_cache = false;
    cfg->ac_attr_timeout_set = 0;
    cfg->nullpath_ok = false;

    start_thallium_engine();
    start_profile_snapshot_thread();

    Metric::stop_operation(OperationFunction::init, start, 0);
    return ServerLocalCacheProvider::my_engine;
}

static void cu_fuse_destroy(void* private_data) {
    LOG(DEBUG) << " " << std::endl;
    auto start = Metric::start_operation(OperationFunction::destroy);
    stop_profile_snapshot_thread();

    Util::log_profiling_metrics(Constants::output_filename_path, "pre-destroy");

    auto engine = static_cast<tl::engine*>(private_data);
    engine->finalize();

    Metric::stop_operation(OperationFunction::destroy, start, Constants::fs_operation_success);
    Util::log_profiling_metrics(Constants::output_filename_path);
    Util::reset_fs();
}

static int cu_fuse_readlink(const char* path_, char* buf, const size_t size) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::readlink, path_);
    if(Util::is_recursive_path_string(path_string)) {
        LOG(WARNING) << "path_string was recursive: " << path_string << std::endl;
        return Metric::stop_cache_operation(OperationFunction::readlink, OperationResult::neg,
        ProfileSource::bypass, path_string, start, -ENOENT);
    }

    if(size == 0) {
        return Metric::stop_operation(OperationFunction::readlink, start, -EINVAL);
    }
    const int res = static_cast<int>(readlink(path_string.c_str(), buf, size - 1));
    if(res == -1) {
        return Metric::stop_operation(OperationFunction::readlink, start, -errno);
    }

    buf[res] = '\0';

    return Metric::stop_operation(OperationFunction::readlink, start, Constants::fs_operation_success);
}

#ifdef HAVE_SETXATTR
static int cu_fuse_getxattr(const char* path_, const char* name, char* value, size_t size) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::getxattr, path_);

    int res = lgetxattr(path_string.c_str(), name, value, size);
    if(res == -1) {
        return Metric::stop_operation(OperationFunction::getxattr, start, -errno);
    }

    return Metric::stop_operation(OperationFunction::getxattr, start, res);
    ;
}
#endif

// clang-format off
#define FUSE_METRIC_CALLBACK(op, expression) do { auto metric_start=Metric::start_operation(OperationFunction::op); auto metric_result=(expression); return Metric::stop_operation(OperationFunction::op,metric_start,metric_result); } while(false)
static int cu_fuse_open(const char*, struct fuse_file_info* fi) { FUSE_METRIC_CALLBACK(open, FusePolicy::open_result(fi->flags)); }
#define READ_ONLY_MUTATION(name, op, signature) static int name signature { FUSE_METRIC_CALLBACK(op, FusePolicy::mutation_result()); }
#ifdef AT_SYMLINK_NOFOLLOW
#define COPPER_ACCESS_FLAGS (AT_EACCESS | AT_SYMLINK_NOFOLLOW)
#else
#define COPPER_ACCESS_FLAGS AT_EACCESS
#endif
READ_ONLY_MUTATION(cu_fuse_mknod, mknod, (const char*, mode_t, dev_t))
READ_ONLY_MUTATION(cu_fuse_mkdir, mkdir, (const char*, mode_t))
READ_ONLY_MUTATION(cu_fuse_unlink, unlink, (const char*))
READ_ONLY_MUTATION(cu_fuse_rmdir, rmdir, (const char*))
READ_ONLY_MUTATION(cu_fuse_symlink, symlink, (const char*, const char*))
READ_ONLY_MUTATION(cu_fuse_rename, rename, (const char*, const char*, unsigned int))
READ_ONLY_MUTATION(cu_fuse_link, link, (const char*, const char*))
READ_ONLY_MUTATION(cu_fuse_chmod, chmod, (const char*, mode_t, struct fuse_file_info*))
READ_ONLY_MUTATION(cu_fuse_chown, chown, (const char*, uid_t, gid_t, struct fuse_file_info*))
READ_ONLY_MUTATION(cu_fuse_truncate, truncate, (const char*, off_t, struct fuse_file_info*))
static int cu_fuse_write(const char*, const char*, size_t, off_t, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(write, FusePolicy::mutation_result()); }
static int cu_fuse_statfs(const char* path_, struct statvfs* stbuf) { const auto p=Util::rel_to_abs_path(path_); int rc=statvfs(p.c_str(),stbuf); int e=errno; FUSE_METRIC_CALLBACK(statfs, rc==0?0:-e); }
static int cu_fuse_flush(const char*, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(flush, 0); }
static int cu_fuse_release(const char*, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(release, 0); }
static int cu_fuse_fsync(const char*, int, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(fsync, 0); }
#ifdef HAVE_SETXATTR
READ_ONLY_MUTATION(cu_fuse_setxattr, setxattr, (const char*, const char*, const char*, size_t, int))
static int cu_fuse_listxattr(const char*, char*, size_t) { FUSE_METRIC_CALLBACK(listxattr, FusePolicy::optional_result()); }
READ_ONLY_MUTATION(cu_fuse_removexattr, removexattr, (const char*, const char*))
#endif
static int cu_fuse_opendir(const char*, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(opendir, 0); }
static int cu_fuse_releasedir(const char*, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(releasedir, 0); }
static int cu_fuse_fsyncdir(const char*, int, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(fsyncdir, 0); }
static int cu_fuse_access(const char* path_, int mask) { if(mask&W_OK) FUSE_METRIC_CALLBACK(access, -EROFS); const auto p=Util::rel_to_abs_path(path_); errno=0; int rc=faccessat(AT_FDCWD,p.c_str(),mask,COPPER_ACCESS_FLAGS); int e=errno; FUSE_METRIC_CALLBACK(access, FusePolicy::access_result(mask,rc==0?0:e)); }
READ_ONLY_MUTATION(cu_fuse_create, create, (const char*, mode_t, struct fuse_file_info*))
static int cu_fuse_lock(const char*, struct fuse_file_info*, int, struct flock*) { FUSE_METRIC_CALLBACK(lock, FusePolicy::optional_result()); }
READ_ONLY_MUTATION(cu_fuse_utimens, utimens, (const char*, const struct timespec[2], struct fuse_file_info*))
static int cu_fuse_bmap(const char*, size_t, uint64_t*) { FUSE_METRIC_CALLBACK(bmap, FusePolicy::optional_result()); }
static int cu_fuse_poll(const char*, struct fuse_file_info*, struct fuse_pollhandle*, unsigned*) { FUSE_METRIC_CALLBACK(poll, FusePolicy::optional_result()); }
static int cu_fuse_write_buf(const char*, struct fuse_bufvec*, off_t, struct fuse_file_info*) { FUSE_METRIC_CALLBACK(write_buf, FusePolicy::mutation_result()); }

static int cu_fuse_flock(const char*, struct fuse_file_info*, int) { FUSE_METRIC_CALLBACK(flock, FusePolicy::optional_result()); }
READ_ONLY_MUTATION(cu_fuse_fallocate, fallocate, (const char*, int, off_t, off_t, struct fuse_file_info*))
static ssize_t cu_fuse_copy_file_range(const char*, struct fuse_file_info*, off_t, const char*, struct fuse_file_info*, off_t, size_t, int) { auto start=Metric::start_operation(OperationFunction::copy_file_range); const ssize_t result=FusePolicy::mutation_result(); Metric::stop_operation(OperationFunction::copy_file_range,start,static_cast<int>(result)); return result; }
static off_t cu_fuse_lseek(const char* path_, off_t off, int whence, struct fuse_file_info*) { auto start=Metric::start_operation(OperationFunction::lseek); struct stat s{}; const auto p=Util::rel_to_abs_path(path_); int rc=lstat(p.c_str(),&s); int e=errno; const off_t result=rc?-e:FusePolicy::lseek_result(s.st_size,off,whence); Metric::stop_operation(OperationFunction::lseek,start,result<0?static_cast<int>(result):0); return result; }
// clang-format on

static constexpr struct fuse_operations cu_fuse_oper = {
.getattr = cu_fuse_getattr,
.readlink = cu_fuse_readlink,
.mknod = cu_fuse_mknod,
.mkdir = cu_fuse_mkdir,
.unlink = cu_fuse_unlink,
.rmdir = cu_fuse_rmdir,
.symlink = cu_fuse_symlink,
.rename = cu_fuse_rename,
.link = cu_fuse_link,
.chmod = cu_fuse_chmod,
.chown = cu_fuse_chown,
.truncate = cu_fuse_truncate,
.open = cu_fuse_open,
.read = cu_fuse_read,
.write = cu_fuse_write,
.statfs = cu_fuse_statfs,
.flush = cu_fuse_flush,
.release = cu_fuse_release,
.fsync = cu_fuse_fsync,
#ifdef HAVE_SETXATTR
.setxattr = cu_fuse_setxattr,
.getxattr = cu_fuse_getxattr,
.listxattr = cu_fuse_listxattr,
.removexattr = cu_fuse_removexattr,
#endif
.opendir = cu_fuse_opendir,
.readdir = cu_fuse_readdir,
.releasedir = cu_fuse_releasedir,
.fsyncdir = cu_fuse_fsyncdir,
.init = cu_fuse_init,
.destroy = cu_fuse_destroy,
.access = cu_fuse_access,
.create = cu_fuse_create,
.lock = cu_fuse_lock,
.utimens = cu_fuse_utimens,
.bmap = cu_fuse_bmap,
.ioctl = cu_fuse_ioctl,
.poll = cu_fuse_poll,
// NOTE: setting these to nullptr causes fallback to read/write respectively
.write_buf = cu_fuse_write_buf,
.read_buf = nullptr,
.flock = cu_fuse_flock,
.fallocate = cu_fuse_fallocate,
.copy_file_range = cu_fuse_copy_file_range,
.lseek = cu_fuse_lseek,
};

int CuFuse::cu_hello_main(int argc, char* argv[]) {
    // Start with no sinks so argument parsing and startup do not leak logs
    // before the user-facing log level has been processed.
    AixLog::Log::init({});

    char char_hostname[1024]{};
    if(gethostname(char_hostname, sizeof(char_hostname) - 1) != 0) {
        throw std::system_error(errno, std::generic_category(), "gethostname failed");
    }
    char_hostname[sizeof(char_hostname) - 1] = '\0';
    Constants::my_hostname = std::string(char_hostname);
    Constants::pid = std::to_string(getpid());

    auto new_args = Util::process_args(argc, argv);
    std::filesystem::create_directories(Constants::logs_dir());
    std::filesystem::create_directories(std::filesystem::path(Constants::tables_dir()) / Constants::profiling_final_subdir);
    std::filesystem::create_directories(Constants::profiling_final_dir());
    std::filesystem::create_directories(Constants::profiling_cluster_dir());
    Constants::output_filename_path = Constants::logs_dir() + "/" + Constants::get_output_filename(Constants::output_filename_suffix);

    if(Constants::logging_enabled(Constants::log_level)) {
        const auto aixlog_severity = static_cast<AixLog::Severity>(
            Constants::map_user_log_level_to_internal_severity(Constants::log_level));

        if(Constants::log_type == "stdout") {
            AixLog::Log::init({std::make_shared<AixLog::SinkCout>(aixlog_severity)});
        } else if(Constants::log_type == "file") {
            AixLog::Log::init({std::make_shared<AixLog::SinkFile>(aixlog_severity, Constants::output_filename_path)});
        } else if(Constants::log_type == "file_and_stdout") {
            AixLog::Log::init({std::make_shared<AixLog::SinkCout>(aixlog_severity),
            std::make_shared<AixLog::SinkFile>(aixlog_severity, Constants::output_filename_path)});
        } else {
            AixLog::Log::init({});
        }
    } else {
        AixLog::Log::init({});
    }

    LOG(INFO) << "hostname found to be: " << char_hostname << std::endl;

    Constants::copper_address_book_path = Constants::logs_dir() + "/" + Constants::copper_address_book_filename;
    LOG(INFO) << "copper address located at path: " << Constants::copper_address_book_path << std::endl;
    LOG(INFO) << "facility address book filename path: " << Constants::facility_address_book_path << std::endl;
    LOG(INFO) << "prefiltered_address_book: " << (Constants::prefiltered_address_book ? "enabled" : "disabled")
              << std::endl;
    LOG(INFO) << "user-facing log_level: " << Constants::log_level << std::endl;
    LOG(INFO) << "internal logger severity: "
              << Constants::map_user_log_level_to_internal_severity(Constants::log_level) << std::endl;
    LOG(INFO) << "metadata ENOENT TTL (ms): " << Constants::md_enoent_ttl_ms << std::endl;
    LOG(INFO) << "profile_metrics: " << (Constants::profile_metrics ? "enabled" : "disabled") << std::endl;
    LOG(INFO) << "profile_top_n: " << Constants::profile_top_n << std::endl;
    LOG(INFO) << "profile_paths_full: " << (Constants::profile_paths_full ? "enabled" : "disabled") << std::endl;
    LOG(INFO) << "output filename path: " << Constants::output_filename_path << std::endl;

    std::vector<char*> ptrs;
    ptrs.reserve(new_args.size());
    for(std::string& str : new_args) {
        ptrs.push_back(str.data());
    }

    return fuse_main(ptrs.size(), ptrs.data(), &cu_fuse_oper, nullptr);
}
