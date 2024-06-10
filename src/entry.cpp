/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

#include <stdexcept>
#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <optional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "passthrough_helpers.h"

#define LOG_COLOR (1)
#define LOG_LEVEL (7)

#include "lwlog.h"

#define MNT_MIRROR_DIR_NAME "/mnt_mirror"

static int fill_dir_plus                         = 0;
static std::optional<std::string> mnt_mirror_dir = std::nullopt;

static std::string rel_to_abs_path(const char* path) {
    if(!path) {
        lwlog_err("path was null");
        throw new std::runtime_error("path was null");
    }

    if(!mnt_mirror_dir.has_value()) {
        lwlog_err("mnt_mirror_dir was std::nullopt");
        throw new std::runtime_error("mnt_mirror_dir was std::nullopt");
    }

    auto suffix   = std::string(path);
    auto abs_path = mnt_mirror_dir.value() + suffix;

    lwlog_debug("abs_path: %s", abs_path.c_str());

    return abs_path;
}

static void* xmp_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    lwlog_debug();

    (void)conn;
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
    cfg->entry_timeout    = 0;
    cfg->attr_timeout     = 0;
    cfg->negative_timeout = 0;

    return NULL;
}

static int xmp_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug("path: %s", path_string.c_str());

    (void)fi;
    int res;

    res = lstat(path_string.c_str(), stbuf);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char* path, int mask) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = access(path_string.c_str(), mask);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char* path, char* buf, size_t size) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = readlink(path_string.c_str(), buf, size - 1);
    if(res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int xmp_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    DIR* dp;
    struct dirent* de;

    (void)offset;
    (void)fi;
    (void)flags;

    dp = opendir(path_string.c_str());
    if(dp == NULL)
        return -errno;

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino  = de->d_ino;
        st.st_mode = de->d_type << 12;
        if(filler(buf, de->d_name, &st, 0, (fuse_fill_dir_flags)fill_dir_plus))
            break;
    }

    closedir(dp);
    return 0;
}

static int xmp_mknod(const char* path, mode_t mode, dev_t rdev) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = mknod_wrapper(AT_FDCWD, path_string.c_str(), NULL, mode, rdev);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_mkdir(const char* path, mode_t mode) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = mkdir(path_string.c_str(), mode);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char* path) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = unlink(path_string.c_str());
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char* path) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = rmdir(path_string.c_str());
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_symlink(const char* from, const char* to) {
    auto from_string = rel_to_abs_path(from);
    auto to_string   = rel_to_abs_path(to);
    lwlog_debug();

    int res;

    res = symlink(from_string.c_str(), to_string.c_str());
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_rename(const char* from, const char* to, unsigned int flags) {
    auto from_string = rel_to_abs_path(from);
    auto to_string   = rel_to_abs_path(to);
    lwlog_debug();

    int res;

    if(flags)
        return -EINVAL;

    res = rename(from_string.c_str(), to_string.c_str());
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_link(const char* from, const char* to) {
    auto from_string = rel_to_abs_path(from);
    auto to_string   = rel_to_abs_path(to);
    lwlog_debug();

    int res;

    res = link(from_string.c_str(), to_string.c_str());
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    (void)fi;
    int res;

    res = chmod(path_string.c_str(), mode);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    (void)fi;
    int res;

    res = lchown(path_string.c_str(), uid, gid);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    if(fi != NULL)
        res = ftruncate(fi->fh, size);
    else
        res = truncate(path_string.c_str(), size);
    if(res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    (void)fi;
    int res;

    /* don't use utime/utimes since they follow symlinks */
    res = utimensat(0, path_string.c_str(), ts, AT_SYMLINK_NOFOLLOW);
    if(res == -1)
        return -errno;

    return 0;
}
#endif

static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = open(path_string.c_str(), fi->flags, mode);
    if(res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int xmp_open(const char* path, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res;

    res = open(path_string.c_str(), fi->flags);
    if(res == -1)
        return -errno;

    /* Enable direct_io when open has flags O_DIRECT to enjoy the feature
    parallel_direct_writes (i.e., to get a shared lock, not exclusive lock,
    for writes to the same file). */
    if(fi->flags & O_DIRECT) {
        fi->direct_io              = 1;
        fi->parallel_direct_writes = 1;
    }

    fi->fh = res;
    return 0;
}

static int xmp_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int fd;
    int res;

    if(fi == NULL)
        fd = open(path_string.c_str(), O_RDONLY);
    else
        fd = fi->fh;

    if(fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if(res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int xmp_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int fd;
    int res;

    (void)fi;
    if(fi == NULL)
        fd = open(path_string.c_str(), O_WRONLY);
    else
        fd = fi->fh;

    if(fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if(res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static int xmp_statfs(const char* path, struct statvfs* stbuf) {
    auto path_string = rel_to_abs_path(path);

    int res;

    res = statvfs(path_string.c_str(), stbuf);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_release(const char* path, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    (void)path_string.c_str();
    close(fi->fh);
    return 0;
}

static int xmp_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    /* Just a stub.	 This method is optional and can safely be left
       unimplemented */

    (void)path_string.c_str();
    (void)isdatasync;
    (void)fi;
    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char* path, int mode, off_t offset, off_t length, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int fd;
    int res;

    (void)fi;

    if(mode)
        return -EOPNOTSUPP;

    if(fi == NULL)
        fd = open(path_string.c_str(), O_WRONLY);
    else
        fd = fi->fh;

    if(fd == -1)
        return -errno;

    res = -posix_fallocate(fd, offset, length);

    if(fi == NULL)
        close(fd);
    return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char* path, const char* name, const char* value, size_t size, int flags) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res = lsetxattr(path.c_str(), name, value, size, flags);
    if(res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char* path, const char* name, char* value, size_t size) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res = lgetxattr(path_string.c_str(), name, value, size);
    if(res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char* path, char* list, size_t size) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res = llistxattr(path_string.c_str(), list, size);
    if(res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char* path, const char* name) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int res = lremovexattr(path_string.c_str(), name);
    if(res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char* path_in,
struct fuse_file_info* fi_in,
off_t offset_in,
const char* path_out,
struct fuse_file_info* fi_out,
off_t offset_out,
size_t len,
int flags) {
    auto path_in_string  = rel_to_abs_path(path_in);
    auto path_out_string = rel_to_abs_path(path_out);
    lwlog_debug();

    int fd_in, fd_out;
    ssize_t res;

    if(fi_in == NULL)
        fd_in = open(path_in_string.c_str(), O_RDONLY);
    else
        fd_in = fi_in->fh;

    if(fd_in == -1)
        return -errno;

    if(fi_out == NULL)
        fd_out = open(path_out_string.c_str(), O_WRONLY);
    else
        fd_out = fi_out->fh;

    if(fd_out == -1) {
        close(fd_in);
        return -errno;
    }

    res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len, flags);
    if(res == -1)
        res = -errno;

    if(fi_out == NULL)
        close(fd_out);
    if(fi_in == NULL)
        close(fd_in);

    return res;
}
#endif

static off_t xmp_lseek(const char* path, off_t off, int whence, struct fuse_file_info* fi) {
    auto path_string = rel_to_abs_path(path);
    lwlog_debug();

    int fd;
    off_t res;

    if(fi == NULL)
        fd = open(path_string.c_str(), O_RDONLY);
    else
        fd = fi->fh;

    if(fd == -1)
        return -errno;

    res = lseek(fd, off, whence);
    if(res == -1)
        res = -errno;

    if(fi == NULL)
        close(fd);
    return res;
}

static const struct fuse_operations xmp_oper = {
.getattr  = xmp_getattr,
.readlink = xmp_readlink,
.mknod    = xmp_mknod,
.mkdir    = xmp_mkdir,
.unlink   = xmp_unlink,
.rmdir    = xmp_rmdir,
.symlink  = xmp_symlink,
.rename   = xmp_rename,
.link     = xmp_link,
.chmod    = xmp_chmod,
.chown    = xmp_chown,
.truncate = xmp_truncate,
.open     = xmp_open,
.read     = xmp_read,
.write    = xmp_write,
.statfs   = xmp_statfs,
// flush
.release = xmp_release,
.fsync   = xmp_fsync,
#ifdef HAVE_SETXATTR
.setxattr    = xmp_setxattr,
.getxattr    = xmp_getxattr,
.listxattr   = xmp_listxattr,
.removexattr = xmp_removexattr,
#endif
// opendir
.readdir = xmp_readdir,
// releasedir
// fsyncdir
.init = xmp_init,
// destroy
.access = xmp_access,
.create = xmp_create,
// lock
#ifdef HAVE_UTIMENSAT
.utimens = xmp_utimens,
#endif
// bmap
// ioctl
// poll
// write_buf
// read_buf
// flock
#ifdef HAVE_POSIX_FALLOCATE
.fallocate = xmp_fallocate,
#endif
#ifdef HAVE_COPY_FILE_RANGE
.copy_file_range = xmp_copy_file_range,
#endif
.lseek = xmp_lseek,
};

int main(int argc, char* argv[]) {
    enum { MAX_ARGS = 10 };
    int i, new_argc;
    char* new_argv[MAX_ARGS];

    umask(0);
    /* Process the "--plus" option apart */
    for(i = 0, new_argc = 0; (i < argc) && (new_argc < MAX_ARGS); i++) {
        if(!strcmp(argv[i], "--plus")) {
            fill_dir_plus = FUSE_FILL_DIR_PLUS;
        } else {
            new_argv[new_argc++] = argv[i];
        }
    }

    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL) {
        lwlog_info("Current working dir: %s\n", cwd);
        mnt_mirror_dir = std::string(cwd) + MNT_MIRROR_DIR_NAME;
    } else {
        lwlog_err("getcwd() error");
        return 1;
    }

    return fuse_main(new_argc, new_argv, &xmp_oper, NULL);
}
