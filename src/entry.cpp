#include <stdexcept>
#define FUSE_USE_VERSION 31

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fuse.h>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>

#include "aixlog.h"
#include "cache/data_cache_table.h"
#include "cache/md_cache_table.h"
#include "fs/constants.h"
#include "fs/util.h"
#include "metric/cache_event.h"
#include "metric/operations.h"

#define NOT_IMPLEMENTED(func)                                  \
    {                                                          \
        LOG(TRACE) << "function not implemented" << std::endl; \
        Operations::inc_operation(func);                       \
        return Constants::fs_operation_success;                \
    }

#define COLLECT_METRICS 1

static int cu_fuse_getattr(const char* path_, struct stat* stbuf, struct fuse_file_info* fi) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    Operations::inc_operation(OperationFunction::getattr);
    const auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;

    const auto cu_stat_opt{CurCache::md_cache_table.get(path_string)};

    // FIXME: make rpc request or root functionality here
    if(!cu_stat_opt.has_value()) {
        if(lstat(path_string.c_str(), stbuf) == -1) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return -errno;
        }

        CacheEvent::record_md_cache_event(path_string, false);
        Operations::inc_operation_cache_hit(OperationFunction::getattr, false);
        LOG(DEBUG) << "not in cache" << std::endl;

        const auto new_cu_stat = new CuStat{stbuf};
        CurCache::md_cache_table.put_force(path_string, std::move(*new_cu_stat));

        return Constants::fs_operation_success;
    }

    CacheEvent::record_md_cache_event(path_string, true);
    Operations::inc_operation_cache_hit(OperationFunction::getattr, true);

    LOG(DEBUG) << "in cache" << std::endl;
    const auto cu_stat = cu_stat_opt.value();

    cu_stat->cp_to_buf(stbuf);

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::getattr, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    return Constants::fs_operation_success;
}

static int cu_fuse_open(const char* path_, struct fuse_file_info* fi) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    Operations::inc_operation(OperationFunction::open);
    const auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;

    const auto entry_opt = CurCache::md_cache_table.get(path_string);

    // FIXME: make rpc request or root functionality here
    if(!entry_opt.has_value()) {
        const int fd = open(path_string.c_str(), fi->flags);

        if(fd == -1) {
            LOG(WARNING) << "failed to passthrough open" << std::endl;
            return -errno;
        }

        struct stat* new_st{};
        if(stat(path_string.c_str(), new_st) == -1) {
            LOG(WARNING) << "failed to passthrough stat" << std::endl;
            return -errno;
        }

        CacheEvent::record_md_cache_event(path_string, false);
        Operations::inc_operation_cache_hit(OperationFunction::open, false);
        LOG(DEBUG) << "not in cache" << path_string << std::endl;

        const auto new_cu_stat = new CuStat{new_st};
        CurCache::md_cache_table.put_force(path_string, std::move(*new_cu_stat));

        fi->fh = fd;
        return Constants::fs_operation_success;
    }


    CacheEvent::record_md_cache_event(path_string, true);
    Operations::inc_operation_cache_hit(OperationFunction::open, true);

    LOG(DEBUG) << "in cache" << std::endl;
    fi->fh = entry_opt.value()->get_st()->st_ino;

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::open, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    return Constants::fs_operation_success;
}

static int cu_fuse_read(const char* path_, char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;

    const auto entry_opt = CurCache::data_cache_table.get(path_string);
    std::vector<std::byte>* bytes = nullptr;
    bool cache = false;

    LOG(DEBUG) << "requested offset: " << offset << std::endl;
    LOG(DEBUG) << "requested size: " << size << std::endl;

    // Allocate bytes if not in cache
    if(!entry_opt.has_value()) {
        try {
            // Allocate new vector and read data into it
            bytes = new std::vector(Util::read_ent_file(path_string, true));
            cache = true;
        } catch(std::runtime_error&) {
            LOG(WARNING) << "failed to passthrough read" << std::endl;
            return -ENOENT;
        }

        CacheEvent::record_data_cache_event(path_string, false);
        Operations::inc_operation_cache_hit(OperationFunction::read, false);
        LOG(DEBUG) << "not in cache" << std::endl;
    } else {
        CacheEvent::record_data_cache_event(path_string, true);
        Operations::inc_operation_cache_hit(OperationFunction::read, true);

        LOG(DEBUG) << "in cache" << std::endl;
        bytes = entry_opt.value();
    }

    LOG(DEBUG) << "bytes buffer cache size: " << bytes->size() << std::endl;

    int write_size = 0;
    if(offset < static_cast<off_t>(bytes->size())) {
        // Calculate the amount to copy, ensuring not to exceed the bounds of the vector
        size_t copy_size = std::min(static_cast<size_t>(size), bytes->size() - static_cast<size_t>(offset));

        // Copy data from bytes to buf
        std::memcpy(buf, bytes->data() + offset, copy_size);
        write_size = static_cast<int>(copy_size);
    } else {
        LOG(DEBUG) << "offset requested greater than file size" << std::endl;
    }

    LOG(DEBUG) << "total read size: " << write_size << std::endl;

    // Store in cache if newly allocated
    if(cache) {
        CurCache::data_cache_table.put_force(path_string, std::move(*bytes));
        delete bytes; // Release memory after moving to cache
    }

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::read, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());

    return write_size;
}

static int
cu_fuse_readdir(const char* path_, void* buf, const fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    Operations::inc_operation(OperationFunction::readdir);
    const auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;

    const auto& tree_cache_table_entry_opt = CurCache::tree_cache_table.get(path_string);
    std::vector<std::string> entries{};
    bool cache = false;

    // FIXME: make rpc request or root functionality here
    if(!tree_cache_table_entry_opt.has_value()) {
        DIR* dp = opendir(path_string.c_str());
        if(dp == nullptr) {
            LOG(WARNING) << "failed to passthrough readdir" << std::endl;
            return -errno;
        }

        CacheEvent::record_dir_cache_event(path_string, false);
        Operations::inc_operation_cache_hit(OperationFunction::readdir, false);
        LOG(DEBUG) << "not in cache" << std::endl;

        dirent* de;
        while((de = readdir(dp)) != nullptr) {
            entries.emplace_back(de->d_name);
        }
        closedir(dp);

        cache = true;
    } else {
        CacheEvent::record_dir_cache_event(path_string, true);
        Operations::inc_operation_cache_hit(OperationFunction::readdir, true);

        LOG(DEBUG) << "in cache" << std::endl;
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
        CurCache::tree_cache_table.put_force(path_string, std::move(entries));
    }


    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::readdir, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());

    return Constants::fs_operation_success;
}

static int cu_fuse_ioctl(const char* path_, int cmd, void* arg, struct fuse_file_info*, unsigned int flags, void* data) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(TRACE) << " " << std::endl;
    Operations::inc_operation(OperationFunction::ioctl);
    auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;
    LOG(DEBUG) << "cmd: " << cmd << std::endl;

    std::string output;
    std::optional<std::ofstream> fs_stream_opt = std::nullopt;

    switch(cmd) {
    case(Constants::ioctl_log_cache):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_cache_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging cache" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << CurCache::tree_cache_table << std::endl;
        fs_stream_opt.value() << CurCache::data_cache_table << std::endl;
        fs_stream_opt.value() << CurCache::md_cache_table << std::endl;
        break;
    case(Constants::ioctl_clear_cache):
        LOG(INFO) << "clearing cache" << std::endl;
        CurCache::tree_cache_table.cache.clear();
        CurCache::md_cache_table.cache.clear();
        CurCache::md_cache_table.cache.clear();
        break;
    case(Constants::ioctl_log_operation):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_operation_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging operation" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << Operations::log_operation << std::endl;
        break;
    case(Constants::ioctl_log_operation_time):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_operation_time_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging operation time (ms)" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << Operations::log_operation_time << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_hit):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_cache_hit_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging operation cache hit" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << Operations::log_operation_cache_hit << std::endl;
        break;
    case(Constants::ioctl_log_operation_cache_miss):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_cache_miss_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging operation cache miss" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << Operations::log_operation_cache_miss << std::endl;
        break;
    case(Constants::ioctl_clear_operation):
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
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_data_cache_event_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(output.c_str());
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging data cache event" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << CacheEvent::log_data_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_dir_cache_event):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_dir_cache_event_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging dir cache event" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << CacheEvent::log_dir_cache_event << std::endl;
        break;
    case(Constants::ioctl_log_md_cache_event):
        output = std::filesystem::path(path_string).parent_path() += "/" + Constants::log_md_cache_event_output_filename;
        fs_stream_opt = Util::try_get_fstream_from_path(static_cast<const char*>(output.c_str()));
        if(!fs_stream_opt.has_value()) {
            LOG(ERROR) << "failed to open fstream" << std::endl;
            return Constants::fs_operation_success;
        }

        LOG(INFO) << "logging md cache event" << std::endl;
        fs_stream_opt.value() << Util::get_current_datetime << std::endl;
        fs_stream_opt.value() << CacheEvent::log_md_cache_event << std::endl;
        break;
    case(Constants::ioctl_clear_data_cache_event):
        LOG(INFO) << "clearing data cache event" << std::endl;
        CacheEvent::reset_data_cache_event();
        break;
    case(Constants::ioctl_clear_dir_cache_event):
        LOG(INFO) << "clearing dir cache event" << std::endl;
        CacheEvent::reset_dir_cache_event();
        break;
    case(Constants::ioctl_clear_md_cache_event):
        LOG(INFO) << "clearing md cache event" << std::endl;
        CacheEvent::reset_md_cache_event();
        break;
    default: LOG(WARNING) << "unknown cmd" << std::endl; break;
    }

    const auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::ioctl, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    return Constants::fs_operation_success;
}

static void* cu_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    Operations::inc_operation(OperationFunction::init);

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
    cfg->no_rofd_flush = true;

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
    cfg->parallel_direct_writes = false;

    // DOCS: The remaining options are used by libfuse internally and should not be touched.
    // cfg->show_help

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::init, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    return nullptr;
}

static void cu_fuse_destroy(void* private_data) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG(DEBUG) << " " << std::endl;
    Operations::inc_operation(OperationFunction::destroy);

    CurCache::data_cache_table.cache.clear();
    CurCache::md_cache_table.cache.clear();
    CurCache::tree_cache_table.cache.clear();
    CacheEvent::reset_data_cache_event();
    CacheEvent::reset_dir_cache_event();
    CacheEvent::reset_md_cache_event();

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(
    OperationFunction::destroy, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
}

// clang-format off
static int cu_fuse_readlink(const char* path_, char* buf, const size_t size) NOT_IMPLEMENTED(OperationFunction::readlink)
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
static int cu_fuse_write(const char* path_, const char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::write)
static int cu_fuse_statfs(const char* path_, struct statvfs* stbuf) NOT_IMPLEMENTED(OperationFunction::statfs)
static int cu_fuse_flush(const char*, struct fuse_file_info*) NOT_IMPLEMENTED(OperationFunction::flush)
static int cu_fuse_release(const char* path_, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::release)
static int cu_fuse_fsync(const char* path_, const int isdatasync, struct fuse_file_info* fi) NOT_IMPLEMENTED(OperationFunction::fsync)
static int cu_fuse_setxattr(const char*, const char*, const char*, size_t, int) NOT_IMPLEMENTED(OperationFunction::setxattr)
static int cu_fuse_getxattr(const char*, const char*, char*, size_t) NOT_IMPLEMENTED(OperationFunction::getxattr)
static int cu_fuse_listxattr(const char*, char*, size_t) NOT_IMPLEMENTED(OperationFunction::listxattr)
static int cu_fuse_removexattr(const char*, const char*) NOT_IMPLEMENTED(OperationFunction::removexattr)
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
.setxattr = cu_fuse_setxattr,
.getxattr = cu_fuse_getxattr,
.listxattr = cu_fuse_listxattr,
.removexattr = cu_fuse_removexattr,
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

int main(const int argc, const char* argv[]) {
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace);
    LOG(TRACE) << " " << std::endl;

    auto new_args{Util::process_args(argc, argv)};

    std::vector<char*> ptrs;
    ptrs.reserve(new_args.size());
    for(std::string& str : new_args) {
        ptrs.push_back(str.data());
    }

    return fuse_main(ptrs.size(), ptrs.data(), &cu_fuse_oper, nullptr);
}
