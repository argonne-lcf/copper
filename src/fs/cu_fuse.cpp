#include "cu_fuse.h"

namespace tl = thallium;

tl::engine serverEngine;
tl::remote_procedure rpc_lstat;
tl::remote_procedure rpc_readfile;
tl::remote_procedure rpc_readdir;

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
        TIME_RPC(ServerLocalCacheProvider::lstat_final_return_type rpc_lstat_response = rpc_lstat.on(engine->self())(true, path_string));

        if(rpc_lstat_response != 0) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::getattr, OperationResult::neg,
            CacheEvent::md_cache_event_table, path_string, start, rpc_lstat_response);
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

    const auto entry_opt = CacheTables::data_cache_table.get(path_string);
    std::vector<std::byte>* bytes = nullptr;
    bool cache = false;

    LOG(DEBUG) << "requested offset: " << offset << std::endl;
    LOG(DEBUG) << "requested size: " << size << std::endl;

    if(!entry_opt.has_value()) {
        const tl::engine* engine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        LOG(INFO, CU_FUSE_RPC_DATA) << __FUNCTION__ << " from client running at address: " << engine->self() << std::endl;
        LOG(INFO, CU_FUSE_RPC_DATA) << __FUNCTION__ << " cu_fuse_read thread id: " << pthread_self() << std::endl;
        TIME_RPC(ServerLocalCacheProvider::read_final_return_type rpc_readfile_response = rpc_readfile.on(engine->self())(true, path_string));

        if(rpc_readfile_response != 0) {
            LOG(WARNING) << "failed to passthrough readfile" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::read, OperationResult::neg,
            CacheEvent::data_cache_event_table, path_string, start, -rpc_readfile_response);
        }

        bytes =  CacheTables::data_cache_table.get(path_string).value();
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
        TIME_RPC(ServerLocalCacheProvider::readdir_final_return_type rpc_readdir_response = rpc_readdir.on(engine->self())(true, path_string));

        if(rpc_readdir_response != Constants::fs_operation_success) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return Metric::stop_cache_operation(OperationFunction::readdir, OperationResult::neg,
            CacheEvent::md_cache_event_table, path_string, start, -rpc_readdir_response);
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

        IOCTL_GET_FS_STREAM(Constants::log_cache_tables_output_filename);
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

        IOCTL_GET_FS_STREAM(Constants::log_operation_output_filename);
        fs_stream_opt.value() << Operations::log_operation << std::endl;
        break;
    case(Constants::ioctl_log_operation_time):
        LOG(INFO) << "logging operation time (ms)" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::log_operation_time_output_filename);
        fs_stream_opt.value() << Operations::log_operation_time << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_hit):
        LOG(INFO) << "logging operation cache hit" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::log_cache_hit_output_filename);
        fs_stream_opt.value() << Operations::log_operation_cache_hit << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_miss):
        LOG(INFO) << "logging operation cache miss" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::log_cache_miss_output_filename);
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

        IOCTL_GET_FS_STREAM(Constants::log_data_cache_event_output_filename);
        fs_stream_opt.value() << CacheEvent::log_data_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_tree_cache_event):
        LOG(INFO) << "logging tree cache event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::log_tree_cache_event_output_filename);
        fs_stream_opt.value() << CacheEvent::log_tree_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_md_cache_event):
        LOG(INFO) << "logging md cache event" << std::endl;

        IOCTL_GET_FS_STREAM(Constants::log_md_cache_event_output_filename);
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

        IOCTL_GET_FS_STREAM(Constants::log_operation_cache_neg_output_filename);
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

        IOCTL_GET_FS_STREAM(Constants::log_ioctl_cache_event_output_filename);
        fs_stream_opt.value() << IoctlEvent::log_ioctl_event << std::endl;
        break;
    case(Constants::ioctl_clear_ioctl_event):
        LOG(INFO) << "clearing ioctl event" << std::endl;
        IoctlEvent::reset_ioctl_event();
        break;
    case(Constants::ioctl_get_data_cache_size):
        LOG(INFO) << "logging data cache size" << std::endl;
        IOCTL_GET_FS_STREAM(Constants::log_data_cache_size_output_filename);
        fs_stream_opt.value() << DataCacheTable::get_data_size_metrics << std::endl;
        break;
    default:
        LOG(DEBUG) << "external ioctl with cmd: " << cmd << std::endl;
        Operations::inc_operation(OperationFunction::ext_getattr);
        IoctlEvent::record_ioctl_event(path_string, cmd);
        break;
    }

    return Metric::stop_operation(OperationFunction::ioctl, start, Constants::fs_operation_success);
}

static void* cu_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    LOG(DEBUG) << " " << std::endl;
    auto start = Metric::start_operation(OperationFunction::init);

    // NOTE: docs (https://libfuse.github.io/doxygen/structfuse__config.html#a3e84d36c87733fcafc594b18a6c3dda8)

    // DOCS: If set_gid is non-zero, the st_gid attribute of each file is overwritten with the
    // value of gid.
    // cfg->set_gid

    // DOCS: If set_uid is non-zero, the st_uid attribute of each file is overwritten with the
    // value of uid.
    // cfg->set_uid

    // DOCS: If set_mode is non-zero, the any permissions bits set in umask are unset in the st_mode attribute of
    // each file.
    // cfg->set_mode

    // DOCS: The timeout in seconds for which name lookups will be cached.
    cfg->entry_timeout = 0; // std::numeric_limits<double>::max();

    // DOCS: The timeout in seconds for which a negative lookup will be cached. This means,
    // that if file did not exist (lookup returned ENOENT), the lookup will only be redone
    // after the timeout, and the file/directory will be assumed to not exist until then. A
    // value of zero means that negative lookups are not cached.
    cfg->negative_timeout = 0; // std::numeric_limits<double>::max();

    // DOCS: The timeout in seconds for which file/directory attributes (as returned by e.g.
    // the getattr handler) are cached.
    cfg->attr_timeout = 0; // std::numeric_limits<double>::max();

    // NOTE: Allow requests to be interrupted
    // cfg->intr

    // DOCS: The timeout in seconds for which file attributes are cached for the purpose of
    // checking if auto_cache should flush the file data on open.
    cfg->intr_signal = 0; // std::numeric_limits<double>::max();

    // DOCS: Normally, FUSE assigns inodes to paths only for as long as the kernel is aware of
    // them. With this option inodes are instead remembered for at least this many seconds. This
    // will require more memory, but may be necessary when using applications that make use of
    // inode numbers.
    // cfg->remember = std::numeric_limits<int>::max();

    // DOCS: The default behavior is that if an open file is deleted, the file is renamed to a
    // hidden file (.fuse_hiddenXXX), and only removed when the file is finally released. This
    // relieves the filesystem implementation of having to deal with this problem. This option
    // disables the hiding behavior, and files are removed immediately in an unlink operation (or
    // in a rename operation which overwrites an existing file).
    // It is recommended that you not use the hard_remove option. When hard_remove is set, the
    // following libc functions fail on unlinked files (returning errno of ENOENT): read(2),
    // write(2), fsync(2), close(2), f*xattr(2), ftruncate(2), fstat(2), fchmod(2), fchown(2)
    // cfg->hard_remove

    // DOCS: Honor the st_ino field in the functions getattr() and fill_dir(). This value is used
    // to fill in the st_ino field in the stat(2), lstat(2), fstat(2) functions and the d_ino
    // field in the readdir(2) function. The filesystem does not have to guarantee uniqueness,
    // however some applications rely on this value being unique for the whole filesystem.
    // Note that this does not affect the inode that libfuse and
    // the kernel use internally(also called the "nodeid")
    cfg->use_ino = false;

    // DOCS: If use_ino option is not given, still try to fill in the d_ino field in readdir(2).
    // If the name was previously looked up, and is still in the cache, the inode number found
    // there will be used. Otherwise it will be set to -1. If use_ino option is given, this
    // option is ignored.
    cfg->readdir_ino = false;

    // DOCS: This option disables the use of page cache (file content cache) in the kernel for
    // this filesystem. This has several affects:
    // Each read(2) or write(2) system call will initiate one or more read or write operations,
    // data will not be cached in the kernel. The return value of the read() and write() system
    // calls will correspond to the return values of the read and write operations. This is useful
    // for example if the file size is not known in advance (before reading it). Internally,
    // enabling this option causes fuse to set the direct_io field of struct fuse_file_info
    // - overwriting any value that was put there by the file system.
    cfg->direct_io = true;

    // DOCS: This option disables flushing the cache of the file contents on every open(2).
    // This should only be enabled on filesystems where the file data is never changed externally
    // (not through the mounted FUSE filesystem). Thus it is not suitable for network filesystems
    // and other intermediate filesystems
    // NOTE: if this option is not specified (and neither direct_io) data is still cached after
    // the open(2), so a read(2) system call will not always initiate a read operation.
    // Internally, enabling this option causes fuse to set the keep_cache field of struct
    // fuse_file_info - overwriting any value that was put there by the file system.
    cfg->kernel_cache = true;

    // DOCS: This option is an alternative to kernel_cache. Instead of unconditionally keeping
    // cached data, the cached data is invalidated on open(2) if if the modification time or the
    // size of the file has changed since it was last opened.
    cfg->auto_cache = false;

    // DOCS: By default, fuse waits for all pending writes to complete and calls the FLUSH
    // operation on close(2) of every fuse fd. With this option, wait and FLUSH are not done for
    // read-only fuse fd, similar to the behavior of NFS/SMB clients.
    // cfg->no_rofd_flush = true; #uncomment this later when we have latest fuse installed on sunspot

    // DOCS: The timeout in seconds for which file attributes are cached for the purpose of
    // checking if auto_cache should flush the file data on open.
    cfg->ac_attr_timeout_set = 0; // std::numeric_limits<int>::max();

    // DOCS: If this option is given the file-system handlers for the following operations will
    // not receive path information: read, write, flush, release, fallocate, fsync, readdir,
    // releasedir, fsyncdir, lock, ioctl and poll.
    // For the truncate, getattr, chmod, chown and utimens operations the path will be provided
    // only if the struct fuse_file_info argument is NULL.
    cfg->nullpath_ok = false;

    // DOCS: Allow parallel direct-io writes to operate on the same file.
    // FUSE implementations which do not handle parallel writes on same file/region
    // should NOT enable this option at all as it might lead to data inconsistencies.
    // For the FUSE implementations which have their own mechanism of cache/data integrity
    // are beneficiaries of this setting as it now open doors to parallel writes on the same
    // file (without enabling this setting, all direct writes on the same file are serialized,
    // resulting in huge data bandwidth loss).
    // cfg->parallel_direct_writes = false;  #uncomment this later when we have latest fuse installed on sunspot


    // DOCS: The remaining options are used by libfuse internally and should not be touched.
    // cfg->show_help

    Metric::stop_operation(OperationFunction::init, start, Constants::fs_operation_success);
    if(fuse_get_context()->private_data == nullptr) {
        LOG(FATAL) << "my fuse_get_context()->private_data was null!";
        throw std::runtime_error("my fuse_get_context()->private_data was null!");
    }

    return fuse_get_context()->private_data;
}

static void cu_fuse_destroy(void* private_data) {
    LOG(DEBUG) << " " << std::endl;
    auto start = Metric::start_operation(OperationFunction::destroy);

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

int CuFuse::cu_hello_main(int argc, char* argv[], void* userdata) {
    return fuse_main(argc, argv, &cu_fuse_oper, userdata);
}
