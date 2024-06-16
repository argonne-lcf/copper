/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

#include <exception>
#include <stdexcept>
#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <optional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "aixlog.h"
#include "cache/data_cache_table.h"
#include "cache/md_cache_table.h"
#include "fs/constants.h"
#include "fs/util.h"
#include "passthrough_helpers.h"

static void* cu_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    LOG(TRACE) << " " << std::endl;

    cfg->use_ino = 1;

    /* parallel_direct_writes feature depends on direct_io features.
   To make parallel_direct_writes valid, need either set cfg->direct_io
   in current function (recommended in high level API) or set fi->direct_io
   in xmp_create() or xmp_open(). */
    // cfg->direct_io = 1;
    cfg->parallel_direct_writes = 1;

    /* Pick up changes from lower filesystem right away. This is
       also necessary for better hardlink support. When the kernel
       calls the unlink() handler, it does not know the inode of
       the to-be-removed entry and can therefore not invalidate
       the cache of the associated inode - resulting in an
       incorrect st_nlink value being reported for any remaining
       hardlinks to this inode. */
    cfg->entry_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->negative_timeout = 0;

    Util::cache_target_path();

    return nullptr;
}

static int cu_fuse_getattr(const char* path_, struct stat* stbuf, struct fuse_file_info* fi) {
    LOG(TRACE) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};
    LOG(DEBUG) << "path_string: " << path_string << std::endl;

    const auto cu_stat_opt{CurCache::md_cache_table.get(path_string)};

    if(!cu_stat_opt.has_value()) {
        LOG(DEBUG) << "path_string not found" << std::endl;
        return -ENOENT;
    } else {
        LOG(DEBUG) << "path_string found" << std::endl;
    }

    const auto& cu_stat{cu_stat_opt.value()};
    cu_stat->cp_to_buf(stbuf);

    return Constants::fs_operation_success;
}

static int cu_fuse_access(const char* path_, const int mask) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    return Constants::fs_operation_success;
}

static int cu_fuse_readlink(const char* path_, char* buf, const size_t size) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    const ssize_t res{readlink(path_string.c_str(), buf, size - 1)};
    if(res == -1) {
        return -errno;
    }

    buf[res] = '\0';

    return Constants::fs_operation_success;
}

static int
cu_fuse_readdir(const char* path_, void* buf, const fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    LOG(TRACE) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    const auto& tree_cache_table_entry_opt = CurCache::tree_cache_table.get(path_string);

    if(!tree_cache_table_entry_opt.has_value()) {
        return -ENOENT;
    }

    const fuse_fill_dir_flags fill_dir{(Constants::fill_dir_plus.has_value()) ? FUSE_FILL_DIR_PLUS : static_cast<fuse_fill_dir_flags>(NULL)};
    filler(buf, ".", nullptr, 0, fill_dir);
    filler(buf, "..", nullptr, 0, fill_dir);

    const auto tree_cache_table_entry = tree_cache_table_entry_opt.value();
    for(const auto& cur_path_string: *tree_cache_table_entry) {
        const auto cu_stat_opt = CurCache::md_cache_table.get(cur_path_string);

        if(!cu_stat_opt.has_value()) {
            LOG(WARNING) << "cu_stat not found for entry: " << cur_path_string << std::endl;
        }

        const auto entry_cstr = Util::deep_cpy_string(Util::get_base_of_path(cur_path_string));
        LOG(DEBUG) << "filling entry: " << entry_cstr << std::endl;

        if(filler(buf, entry_cstr, cu_stat_opt.value()->get_st_cpy(), 0, fill_dir) == 1) {
            LOG(ERROR) << "filler returned 1" << std::endl;
        }
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_mknod(const char* path_, const mode_t mode, const dev_t rdev) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    if(mknod_wrapper(AT_FDCWD, path_string.c_str(), nullptr, mode, rdev) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_mkdir(const char* path_, const mode_t mode) {
    LOG(DEBUG) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    /*CuStat cu_stat;
    const auto st = cu_stat.get_st();
    std::memset(st, 0, sizeof(struct stat));

    st->st_mode = mode;
    st->st_ino = Util::gen_inode();

    LOG(DEBUG) << "adding to md and tree cache: " << path_string << std::endl;
    CurCache::md_cache_table.put_force(path_string, std::move(cu_stat));
    CurCache::tree_cache_table.put_force(path_string, std::vector<std::string>());

    const std::filesystem::path path = path_string;
    const auto parent_path = path.parent_path().string();
    LOG(DEBUG) << "parent_path was: " << parent_path << std::endl;
    const auto tree_entry = CurCache::tree_cache_table.get(parent_path);

    if(!tree_entry.has_value()) {
        LOG(ERROR) << "failed to find parent_path: " << parent_path << std::endl;
        return -ENOENT;
    }

    tree_entry.value()->push_back(path_string);*/

    return Constants::fs_operation_success;
}

static int cu_fuse_unlink(const char* path_) {
    LOG(DEBUG) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    Util::remove_entry_from_cache(path_string);

    return Constants::fs_operation_success;
}

static int cu_fuse_rmdir(const char* path_) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    if(rmdir(path_string.c_str()) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_symlink(const char* from_, const char* to_) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto from_string{Util::rel_to_abs_path(from_)};
    const auto to_string{Util::rel_to_abs_path(to_)};

    if(symlink(from_string.c_str(), to_string.c_str()) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_rename(const char* from_, const char* to_, unsigned int flags) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto from_string{Util::rel_to_abs_path(from_)};
    const auto to_string{Util::rel_to_abs_path(to_)};

    return Constants::fs_operation_success;
}

static int cu_fuse_link(const char* from_, const char* to_) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto from_string{Util::rel_to_abs_path(from_)};
    const auto to_string{Util::rel_to_abs_path(to_)};

    if(link(from_string.c_str(), to_string.c_str()) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_chmod(const char* path_, const mode_t mode, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    if(chmod(path_string.c_str(), mode) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_chown(const char* path_, const uid_t uid, const gid_t gid, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    if(chown(path_string.c_str(), uid, gid) == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_truncate(const char* path_, const off_t offset, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    int res{};

    if(fi != nullptr) {
        res = ftruncate(fi->fh, offset);
    } else {
        res = truncate(path_string.c_str(), offset);
    }

    if(res == -1) {
        return -errno;
    }

    return Constants::fs_operation_success;
}

static int cu_fuse_create(const char* path_, const mode_t mode, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    CuStat cu_stat;

    const auto st = cu_stat.get_st();

    // FIMXE: set correct uid gid etc.
    std::memset(st, 0, sizeof(struct stat));

    st->st_mode = mode;
    st->st_ino = Util::gen_inode();

    CurCache::md_cache_table.put_force(path_string, std::move(cu_stat));

    const std::filesystem::path path = path_string;
    const auto parent_path = path.parent_path().string();
    LOG(DEBUG) << "parent_path was: " << parent_path << std::endl;
    const auto tree_entry = CurCache::tree_cache_table.get(parent_path);

    if(!tree_entry.has_value()) {
        LOG(ERROR) << "failed to find parent_path: " << parent_path << std::endl;
        return -ENOENT;
    }

    tree_entry.value()->push_back(path_string);

    if(S_ISDIR(st->st_mode)) {
        LOG(DEBUG) << "dir creation requested" << std::endl;
        CurCache::tree_cache_table.put_force(path_string, std::vector<std::string>());
    } else {
        LOG(DEBUG) << "file creation requested" << std::endl;
        CurCache::data_cache_table.put_force(path_string, std::pair(std::vector<std::byte>(), 0));
    }

    fi->fh = st->st_ino;

    return Constants::fs_operation_success;
}

static int cu_fuse_open(const char* path_, struct fuse_file_info* fi) {
    LOG(TRACE) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    auto entry_opt = CurCache::md_cache_table.get(path_string);

    if(!entry_opt.has_value()) {
        LOG(ERROR) << "entry not found for path: " << path_string << std::endl;
    }

    if(fi->flags & O_DIRECT) {
        fi->direct_io = 1;
        fi->parallel_direct_writes = 1;
    }

    fi->fh = entry_opt.value()->get_st()->st_ino;

    return Constants::fs_operation_success;
}

static int cu_fuse_read(const char* path_, char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) {
    LOG(DEBUG) << " " << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    auto entry_opt = CurCache::data_cache_table.get(path_string);
    if(!entry_opt.has_value()) {
        LOG(DEBUG) << "file not found: " << path_string << std::endl;
        return -ENOENT;
    }

    const auto data_size = static_cast<int>(entry_opt.value()->first.size());
    const auto data = entry_opt.value()->first.data();

    LOG(DEBUG) << "data_size: " << data_size << std::endl;
    LOG(DEBUG) << "reqested offset: " << offset << std::endl;
    LOG(DEBUG) << "requested size: " << size << std::endl;

    if(offset + size < data_size) {
        mempcpy(buf, (data + offset), size);
    } else {
        mempcpy(buf, (data + offset), data_size);
        return data_size;
    }

    return static_cast<int>(size);
}

static int cu_fuse_write(const char* path_, const char* buf, const size_t size, const off_t offset, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    int fd{};

    if(fi == nullptr) {
        fd = open(path_string.c_str(), O_WRONLY);
    } else {
        fd = fi->fh;
    }

    if(fd == -1) {
        return -errno;
    }

    int res(pwrite(fd, buf, size, offset));
    if(res == -1) {
        res = -errno;
    }

    if(fi == nullptr) {
        close(fd);
    }

    return res;
}

static int cu_fuse_statfs(const char* path_, struct statvfs* stbuf) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    if (statvfs(path_string.c_str(), stbuf) == -1)
        return -errno;

    return Constants::fs_operation_success;
}

static int cu_fuse_release(const char* path_, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    return Constants::fs_operation_success;
}

static int cu_fuse_fsync(const char* path_, const int isdatasync, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    return Constants::fs_operation_success;
}

static int cu_fuse_utimens(const char* path_, const struct timespec tv[2], struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    return Constants::fs_operation_success;
}

static off_t cu_fuse_lseek(const char* path_, const off_t off, const int whence, struct fuse_file_info* fi) {
    LOG(WARNING) << "not implemented!" << std::endl;
    const auto path_string{Util::rel_to_abs_path(path_)};

    int fd{};

    if(fi == nullptr) {
        fd = open(path_string.c_str(), O_RDONLY);
    } else {
        fd = fi->fh;
    }

    if(fd == -1) {
        return -errno;
    }

    off_t res{lseek(fd, off, whence)};
    if(res == -1) {
        res = -errno;
    }

    if(fi == nullptr) {
        close(fd);
    }

    return res;
}

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
.release = cu_fuse_release,
.fsync = cu_fuse_fsync,
.readdir = cu_fuse_readdir,
.init = cu_fuse_init,
.access = cu_fuse_access,
.create = cu_fuse_create,
.utimens = cu_fuse_utimens,
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
