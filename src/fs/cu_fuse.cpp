#include "cu_fuse.h"

namespace tl = thallium;

#define CHECK_RECURSIVE(path_string)                                                   \
    {                                                                                  \
        if(Util::is_recursive_path_string(path_string)) {                              \
            LOG(WARNING) << "path_string was recursive: " << path_string << std::endl; \
            return -ENOENT;                                                            \
        }                                                                              \
    }

#define NOT_IMPLEMENTED(func)                                                        \
    {                                                                                \
        LOG(DEBUG) << " " << std::endl;                                              \
        auto start = Metric::start_operation(func);                                  \
        return Metric::stop_operation(func, start, Constants::fs_operation_success); \
    }

#define NOT_IMPLEMENTED_SPECIFY_RETURN(func, ret)        \
    {                                                    \
        LOG(DEBUG) << " " << std::endl;                  \
        auto start = Metric::start_operation(func);      \
        return Metric::stop_operation(func, start, ret); \
    }

#define CU_FUSE_RPC_DATA "cu_fuse_rpc_data"
#define CU_FUSE_RPC_METADATA "cu_fuse_rpc_metadata"

static int cu_fuse_getattr(const char* path_, struct stat* stbuf, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::getattr, path_);
    CHECK_RECURSIVE(path_string);

    auto cu_stat_opt{CacheTables::md_cache_table.get(path_string)};
    if(!cu_stat_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(INFO, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " from client running at address " << engine->self() << std::endl;
        LOG(INFO, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::lstat_final_return_type rpc_lstat_response =
                             ServerLocalCacheProvider::rpc_lstat.on(engine->self())(true, path_string));

        if(rpc_lstat_response != 0) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::neg,
            CacheEvent::md_cache_event_table, path_string, start, rpc_lstat_response);
        }

        if(!CacheTables::md_cache_table.get(path_string).has_value()) {
            LOG(ERROR) << "expected value to exist after rpc caching" << std::endl;
            assert(0);
        }
    }

    cu_stat_opt = CacheTables::md_cache_table.get(path_string);
    const auto cu_stat = cu_stat_opt.value();
    cu_stat->cp_to_buf(stbuf);

    return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::cache_hit,
    CacheEvent::md_cache_event_table, path_string, start, Constants::fs_operation_success);
}

static int cu_fuse_read(const char* path_, char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::read, path_);
    CHECK_RECURSIVE(path_string);

    LOG(DEBUG) << "requested offset: " << offset << std::endl;
    LOG(DEBUG) << "requested size: " << size << std::endl;

    auto md_entry = CacheTables::md_cache_table.get(path_string);
    if(md_entry.has_value()) {
        struct stat* md_st = (struct stat*)md_entry.value()->get_vec().data();
        if(md_st->st_size >= Constants::max_cacheable_byte_size) {
            LOG(INFO) << "file larger than max cacheable size... going to lustre" << std::endl;

            int fd = open(path_string.c_str(), O_RDONLY);
            int res = pread(fd, buf, size, offset);
            if(res == -1) {
                res = -errno;
            }
            close(fd);

            return Metric::stop_cache_operation(
            OperationFunction::read, OperationResult::neg, CacheEvent::data_cache_event_table, path_string, start, res);
        }
    }

    const auto entry_opt = CacheTables::data_cache_table.get(path_string);
    std::vector<std::byte>* bytes = nullptr;
    bool cache = false;

    if(!entry_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(INFO, CU_FUSE_RPC_DATA) << __FUNCTION__ << " from client running at address: " << engine->self() << std::endl;
        LOG(INFO, CU_FUSE_RPC_DATA) << __FUNCTION__ << " cu_fuse_read thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::read_final_return_type rpc_readfile_response =
                             ServerLocalCacheProvider::rpc_readfile.on(engine->self())(true, path_string));

        if(rpc_readfile_response < 0) {
            LOG(WARNING) << "failed to passthrough readfile" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
            CacheEvent::data_cache_event_table, path_string, start, rpc_readfile_response);
        }

        if(!CacheTables::data_cache_table.get(path_string).has_value()) {
            LOG(ERROR) << "expected value to exist after rpc caching" << std::endl;
            assert(0);
        }

        bytes = CacheTables::data_cache_table.get(path_string).value();
        cache = true;
    } else {
        bytes = entry_opt.value();
    }

    int read_size = 0;
    if(offset < static_cast<off_t>(bytes->size())) {
        // Calculate the amount to copy, ensuring not to exceed the bounds of the vector
        size_t copy_size = std::min(static_cast<size_t>(size), bytes->size() - static_cast<size_t>(offset));

        // Copy data from bytes to buf
        std::memcpy(buf, bytes->data() + offset, copy_size);
        read_size = static_cast<int>(copy_size);
    }

    if(cache) {
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::cache_miss,
        CacheEvent::data_cache_event_table, path_string, start, read_size);
    } else {
        return Metric::stop_cache_operation(OperationFunction::read, OperationResult::cache_hit,
        CacheEvent::data_cache_event_table, path_string, start, read_size);
    }
}

static int
cu_fuse_readdir(const char* path_, void* buf, const fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::readdir, path_);
    CHECK_RECURSIVE(path_string);

    const auto& tree_cache_table_entry_opt = CacheTables::tree_cache_table.get(path_string);
    std::vector<std::string> entries{};
    bool cache = false;

    if(!tree_cache_table_entry_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(INFO, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " from client running at address " << engine->self() << std::endl;
        LOG(INFO, CU_FUSE_RPC_METADATA) << __FUNCTION__ << " thread id: " << pthread_self() << std::endl;
        TIME_RPC_FUSE_THREAD(ServerLocalCacheProvider::readdir_final_return_type rpc_readdir_response =
                             ServerLocalCacheProvider::rpc_readdir.on(engine->self())(true, path_string));

        if(rpc_readdir_response != Constants::fs_operation_success) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::neg,
            CacheEvent::md_cache_event_table, path_string, start, rpc_readdir_response);
        }

        if(!CacheTables::tree_cache_table.get(path_string).has_value()) {
            LOG(ERROR) << "expected value to exist after rpc caching" << std::endl;
            assert(0);
        }

        entries = *CacheTables::tree_cache_table.get(path_string).value();
        cache = true;
    } else {
        entries = *tree_cache_table_entry_opt.value();
    }

    const fuse_fill_dir_flags fill_dir{
    (Constants::fill_dir_plus.has_value()) ? FUSE_FILL_DIR_PLUS : static_cast<fuse_fill_dir_flags>(NULL)};

    for(const auto& cur_path_stem : entries) {
        const auto cur_full_path = std::string(path_string).append("/").append(cur_path_stem);

        if(filler(buf, cur_path_stem.c_str(), nullptr, 0, fill_dir) == 1) {
            LOG(ERROR) << "filler returned 1" << std::endl;
        }
    }

    if(cache) {
        return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::cache_miss,
        CacheEvent::tree_cache_event_table, path_string, start, Constants::fs_operation_success);
    } else {
        return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::cache_hit,
        CacheEvent::tree_cache_event_table, path_string, start, Constants::fs_operation_success);
    }
}

#define IOCTL_GET_FS_STREAM(filename)                                                                  \
    output = std::filesystem::path(path_string).parent_path() += "/" + filename;                       \
    fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());                                   \
    if(!fs_stream_opt.has_value()) {                                                                   \
        LOG(ERROR) << "failed to open fstream" << std::endl;                                           \
        return Metric::stop_operation(OperationFunction::ioctl, start, Constants::fs_operation_error); \
    }

static int cu_fuse_ioctl(const char* path_, int cmd, void* arg, struct fuse_file_info*, unsigned int flags, void* data) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::ioctl, path_);
    CHECK_RECURSIVE(path_string);

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
        CacheTables::tree_cache_table.cache.clear();
        CacheTables::md_cache_table.cache.clear();
        CacheTables::md_cache_table.cache.clear();
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
        Operations::reset_operation_counter();
        break;
    case(Constants::ioctl_clear_operation_cache_hit):
        LOG(INFO) << "clearing operation cache hit" << std::endl;
        Operations::reset_operation_cache_hit();
        break;
    case(Constants::ioctl_clear_operation_cache_miss):
        LOG(INFO) << "clearing operation cache miss" << std::endl;
        Operations::reset_operation_cache_miss();
        break;
    case(Constants::ioctl_clear_operation_time):
        LOG(INFO) << "clearing operation time" << std::endl;
        Operations::reset_operation_timer();
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
        CacheEvent::reset_data_cache_event();
        break;
    case(Constants::ioctl_clear_tree_cache_event):
        LOG(INFO) << "clearing tree cache event" << std::endl;
        CacheEvent::reset_tree_cache_event();
        break;
    case(Constants::ioctl_clear_md_cache_event):
        LOG(INFO) << "clearing md cache event" << std::endl;
        CacheEvent::reset_md_cache_event();
        break;
    case(Constants::ioctl_log_operation_neg): {
        LOG(INFO) << "logging operation neg event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::get_output_filename(Constants::log_operation_cache_neg_output_filename));
        fs_stream_opt.value() << Operations::log_operation_neg << std::endl;
        break;
    }
    case(Constants::ioctl_clear_operation_neg):
        LOG(INFO) << "clearing operation neg event" << std::endl;
        Operations::reset_operation_cache_neg();
        break;
    case(Constants::ioctl_log_all_metrics):
        LOG(INFO) << "loggin all metrics" << std::endl;
        Util::log_all_metrics(path_string);
        break;
    case(Constants::ioctl_reset_fs):
        LOG(INFO) << "resetting filesystem" << std::endl;
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

        LOG(INFO) << "redirecting stderr to output_file" << std::endl;
        if(freopen(Constants::output_filename_path.c_str(), "a", stderr) == nullptr) {
            LOG(ERROR) << "failed to redirect stderr to path: " << Constants::output_filename_path << std::endl;
        };

        auto logger = new TLLogger();
        tl::logger::set_global_logger(logger);
        tl::logger::set_global_log_level(tl::logger::level::trace);

        tl::engine* server_engine;
        try {
            server_engine = new tl::engine{Constants::network_type, THALLIUM_SERVER_MODE, true, Constants::es};
	    LOG(INFO) << "engine started" << std::endl;
        } catch(std::exception& e) {
            LOG(FATAL) << e.what() << std::endl;
            return;
        }

        server_engine->set_logger(logger);
        server_engine->set_log_level(tl::logger::level::debug);

        LOG(INFO) << "using network type: " << Constants::network_type << std::endl;

        if(Constants::nodefile.has_value()) {
            LOG(INFO) << "using nodefile to init addresses" << std::endl;
            LOG(INFO) << "Generating CXI address from nodefile: " << Constants::nodefile.value() << std::endl;
            NodeTree::generate_nodelist_from_nodefile(Constants::nodefile.value());
        } else if(Constants::network_type == "cxi") {
            LOG(INFO) << "parsing cxi network file to init addresses" << std::endl;
            NodeTree::get_hsn0_cxi_addr();

            LOG(INFO) << "address written to address_book... sleeping for synchronization time: " << Constants::address_write_sync_time << std::endl;
            sleep(Constants::address_write_sync_time);

            NodeTree::parse_nodelist_from_address_book();
        } else if(Constants::network_type == "na+sm" || Constants::network_type == "tcp") {
            LOG(INFO) << "using address from server engine to init addresses" << std::endl;
            NodeTree::push_back_address(Constants::my_hostname, server_engine->self());

            LOG(INFO) << "address written to address_book... sleeping for synchronization time: " << Constants::address_write_sync_time << std::endl;
            sleep(Constants::address_write_sync_time);

            NodeTree::parse_nodelist_from_address_book();
        } else {
            LOG(FATAL) << "invalid network type" << std::endl;
            return;
        }

        LOG(INFO) << "building rpc overlay network tree" << std::endl;
        Node::root = NodeTree::build_my_tree(Node::root, ServerLocalCacheProvider::node_address_data);

        LOG(INFO) << "setting parent node addr" << std::endl;
        if(Node::root->addr != std::string(server_engine->self())) {
            std::string parent{};
            NodeTree::get_parent_from_tree(Node::root, server_engine->self(), parent);
            Node::parent_addr = parent;
            LOG(INFO) << "non-root node setting parent addr: " << Node::parent_addr.value() << std::endl;
        } else {
            LOG(INFO) << "root node not setting parent addr" << std::endl;
        }

        NodeTree::print_tree(Node::root);
        int tree_depth = NodeTree::depth(Node::root);
        LOG(INFO) << "the depth of the tree is: " << tree_depth << std::endl;
        NodeTree::pretty_print_tree(Node::root, tree_depth);

        LOG(INFO) << "server running at address: " << server_engine->self() << std::endl;
        server_engine->enable_remote_shutdown();
        ServerLocalCacheProvider::rpc_lstat = server_engine->define("rpc_lstat");
        ServerLocalCacheProvider::rpc_readfile = server_engine->define("rpc_readfile");
        ServerLocalCacheProvider::rpc_readdir = server_engine->define("rpc_readdir");
        new ServerLocalCacheProvider{*server_engine, ServerLocalCacheProvider::node_address_data};

        LOG(INFO) << "setting my_engine" << std::endl;
        ServerLocalCacheProvider::my_engine = server_engine;

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

    Metric::stop_operation(OperationFunction::init, start, 0);
    return ServerLocalCacheProvider::my_engine;
}

static void cu_fuse_destroy(void* private_data) {
    LOG(DEBUG) << " " << std::endl;
    auto start = Metric::start_operation(OperationFunction::destroy);

    auto engine = static_cast<tl::engine*>(private_data);
    engine->finalize();

    Util::reset_fs();

    Metric::stop_operation(OperationFunction::destroy, start, Constants::fs_operation_success);
}

static int cu_fuse_readlink(const char* path_, char* buf, const size_t size) {
    LOG(DEBUG) << " " << std::endl;
    auto [path_string, start] = Metric::start_cache_operation(OperationFunction::readlink, path_);
    CHECK_RECURSIVE(path_string);

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
static int cu_fuse_open(const char* path_, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::open)
static int cu_fuse_mknod(const char* path_, const mode_t mode, const dev_t rdev) NOT_IMPLEMENTED(OperationFunction::mknod)
static int cu_fuse_mkdir(const char* path_, const mode_t mode) NOT_IMPLEMENTED(OperationFunction::mkdir)
static int cu_fuse_unlink(const char* path_) NOT_IMPLEMENTED(OperationFunction::unlink)
static int cu_fuse_rmdir(const char* path_) NOT_IMPLEMENTED(OperationFunction::rmdir)
static int cu_fuse_symlink(const char*, const char*) NOT_IMPLEMENTED(OperationFunction::symlink)
static int cu_fuse_rename(const char* from_, const char* to_, unsigned int flags) NOT_IMPLEMENTED(OperationFunction::rename)
static int cu_fuse_link(const char* from_, const char* to_) NOT_IMPLEMENTED(OperationFunction::link)
static int cu_fuse_chmod(const char* path_, const mode_t mode, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::chmod)
static int cu_fuse_chown(const char* path_, const uid_t uid, const gid_t gid, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::chown)
static int cu_fuse_truncate(const char*, off_t, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::truncate)
// NOTE: need to return write size or some many programs will continue attempting to write
static int cu_fuse_write(const char* path_, const char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi)
    NOT_IMPLEMENTED_SPECIFY_RETURN(OperationFunction::write, size)
static int cu_fuse_statfs(const char* path_, struct statvfs* stbuf) NOT_IMPLEMENTED(OperationFunction::statfs)
static int cu_fuse_flush(const char*, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::flush)
static int cu_fuse_release(const char* path_, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::release)
static int cu_fuse_fsync(const char* path_, const int isdatasync, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::fsync)
#ifdef HAVE_SETXATTR
static int cu_fuse_setxattr(const char*, const char*, const char*, size_t, int) NOT_IMPLEMENTED(OperationFunction::setxattr)
static int cu_fuse_listxattr(const char*, char*, size_t) NOT_IMPLEMENTED(OperationFunction::listxattr)
static int cu_fuse_removexattr(const char*, const char*) NOT_IMPLEMENTED(OperationFunction::removexattr)
#endif
static int cu_fuse_opendir(const char*, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::opendir)
static int cu_fuse_releasedir(const char*, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::releasedir)
static int cu_fuse_fsyncdir(const char*, int, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::fsyncdir)
static int cu_fuse_access(const char* path_, const int mask) NOT_IMPLEMENTED(OperationFunction::access)
static int cu_fuse_create(const char* path_, const mode_t mode, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::create)
static int cu_fuse_lock(const char*, struct fuse_file_info*, int cmd, struct flock*) NOT_IMPLEMENTED(OperationFunction::lock)
static int cu_fuse_utimens(const char* path_, const struct timespec tv[2], struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::utimens)
static int cu_fuse_bmap(const char*, size_t blocksize, uint64_t* idx) NOT_IMPLEMENTED(OperationFunction::bmap)
static int cu_fuse_poll(const char*, struct fuse_file_info*, struct fuse_pollhandle* ph, unsigned* reventsp) NOT_IMPLEMENTED(OperationFunction::poll)
static int cu_fuse_write_buf(const char*, struct fuse_bufvec* buf, off_t off, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::write_buf)
static int cu_fuse_read_buf(const char*, struct fuse_bufvec** bufp, size_t size, off_t off, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::read_buf)
static int cu_fuse_flock(const char*, struct fuse_file_info*, int op) NOT_IMPLEMENTED(OperationFunction::flock)
static int cu_fuse_fallocate(const char*, int, off_t, off_t, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::fallocate)
static ssize_t cu_fuse_copy_file_range(const char* path_in, struct fuse_file_info* fi_in,
                                       off_t offset_in, const char* path_out,
                                       struct fuse_file_info* fi_out, off_t offset_out,
                                       size_t size, int flags) NOT_IMPLEMENTED(OperationFunction::copy_file_range)
static off_t cu_fuse_lseek(const char* path_, const off_t off,
                           const int whence, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::lseek)
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
.write_buf = nullptr,
.read_buf = nullptr,
.flock = cu_fuse_flock,
.fallocate = cu_fuse_fallocate,
.copy_file_range = cu_fuse_copy_file_range,
.lseek = cu_fuse_lseek,
};

int CuFuse::cu_hello_main(int argc, char* argv[]) {
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);
    LOG(DEBUG) << " " << std::endl;

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    Constants::my_hostname = std::string(char_hostname);

    LOG(INFO) << "hostname found to be: " << char_hostname << std::endl;

    auto new_args = Util::process_args(argc, argv);

    Constants::copper_address_book_path = Constants::log_output_dir.value() + "/" + Constants::copper_address_book_filename;
    LOG(INFO) << "copper address located at path: " << Constants::copper_address_book_path << std::endl;

    Constants::pid = std::to_string(getpid());
    Constants::output_filename_path = Constants::log_output_dir.value() + "/" + Constants::get_output_filename(Constants::output_filename_suffix);
    LOG(INFO) << "output filename path: " << Constants::output_filename_path << std::endl;

    if(Constants::log_type == "stdout") {
        AixLog::Log::init({std::make_shared<AixLog::SinkCout>(static_cast<AixLog::Severity>(Constants::log_level))});
    } else if(Constants::log_type == "file") {
        AixLog::Log::init({std::make_shared<AixLog::SinkFile>(static_cast<AixLog::Severity>(Constants::log_level), Constants::output_filename_path)});
    } else if(Constants::log_type == "file_and_stdout") {
        AixLog::Log::init({std::make_shared<AixLog::SinkCout>(static_cast<AixLog::Severity>(Constants::log_level)),
        std::make_shared<AixLog::SinkFile>(static_cast<AixLog::Severity>(Constants::log_level), Constants::output_filename_path)});
    }

    std::vector<char*> ptrs;
    ptrs.reserve(new_args.size());
    for(std::string& str : new_args) {
        ptrs.push_back(str.data());
    }

    return fuse_main(ptrs.size(), ptrs.data(), &cu_fuse_oper, nullptr);
}
