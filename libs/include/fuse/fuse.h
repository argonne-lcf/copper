/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 
  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/
 
#ifndef FUSE_H_
#define FUSE_H_
 
#include "fuse_common.h"
 
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
 
#ifdef __cplusplus
extern "C" {
#endif
 
/* ----------------------------------------------------------- *
 * Basic FUSE API                                              *
 * ----------------------------------------------------------- */
 
struct fuse;
 
enum fuse_readdir_flags {
        FUSE_READDIR_PLUS = (1 << 0)
};
 
enum fuse_fill_dir_flags {
        FUSE_FILL_DIR_PLUS = (1 << 1)
};
 
typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
                                const struct stat *stbuf, off_t off,
                                enum fuse_fill_dir_flags flags);
struct fuse_config {
        int set_gid;
        unsigned int gid;
 
        int set_uid;
        unsigned int uid;
 
        int set_mode;
        unsigned int umask;
 
        double entry_timeout;
 
        double negative_timeout;
 
        double attr_timeout;
 
        int intr;
 
        int intr_signal;
 
        int remember;
 
        int hard_remove;
 
        int use_ino;
 
        int readdir_ino;
 
        int direct_io;
 
        int kernel_cache;
 
        int auto_cache;
 
        int no_rofd_flush;
 
        int ac_attr_timeout_set;
        double ac_attr_timeout;
 
        int nullpath_ok;
        int parallel_direct_writes;
 
        int show_help;
        char *modules;
        int debug;
};
 
 
struct fuse_operations {
        int (*getattr) (const char *, struct stat *, struct fuse_file_info *fi);
 
        int (*readlink) (const char *, char *, size_t);
 
        int (*mknod) (const char *, mode_t, dev_t);
 
        int (*mkdir) (const char *, mode_t);
 
        int (*unlink) (const char *);
 
        int (*rmdir) (const char *);
 
        int (*symlink) (const char *, const char *);
 
        int (*rename) (const char *, const char *, unsigned int flags);
 
        int (*link) (const char *, const char *);
 
        int (*chmod) (const char *, mode_t, struct fuse_file_info *fi);
 
        int (*chown) (const char *, uid_t, gid_t, struct fuse_file_info *fi);
 
        int (*truncate) (const char *, off_t, struct fuse_file_info *fi);
 
        int (*open) (const char *, struct fuse_file_info *);
 
        int (*read) (const char *, char *, size_t, off_t,
                     struct fuse_file_info *);
 
        int (*write) (const char *, const char *, size_t, off_t,
                      struct fuse_file_info *);
 
        int (*statfs) (const char *, struct statvfs *);
 
        int (*flush) (const char *, struct fuse_file_info *);
 
        int (*release) (const char *, struct fuse_file_info *);
 
        int (*fsync) (const char *, int, struct fuse_file_info *);
 
        int (*setxattr) (const char *, const char *, const char *, size_t, int);
 
        int (*getxattr) (const char *, const char *, char *, size_t);
 
        int (*listxattr) (const char *, char *, size_t);
 
        int (*removexattr) (const char *, const char *);
 
        int (*opendir) (const char *, struct fuse_file_info *);
 
        int (*readdir) (const char *, void *, fuse_fill_dir_t, off_t,
                        struct fuse_file_info *, enum fuse_readdir_flags);
 
        int (*releasedir) (const char *, struct fuse_file_info *);
 
        int (*fsyncdir) (const char *, int, struct fuse_file_info *);
 
        void *(*init) (struct fuse_conn_info *conn,
                       struct fuse_config *cfg);
 
        void (*destroy) (void *private_data);
 
        int (*access) (const char *, int);
 
        int (*create) (const char *, mode_t, struct fuse_file_info *);
 
        int (*lock) (const char *, struct fuse_file_info *, int cmd,
                     struct flock *);
 
         int (*utimens) (const char *, const struct timespec tv[2],
                         struct fuse_file_info *fi);
 
        int (*bmap) (const char *, size_t blocksize, uint64_t *idx);
 
#if FUSE_USE_VERSION < 35
        int (*ioctl) (const char *, int cmd, void *arg,
                      struct fuse_file_info *, unsigned int flags, void *data);
#else
        int (*ioctl) (const char *, unsigned int cmd, void *arg,
                      struct fuse_file_info *, unsigned int flags, void *data);
#endif
 
        int (*poll) (const char *, struct fuse_file_info *,
                     struct fuse_pollhandle *ph, unsigned *reventsp);
 
        int (*write_buf) (const char *, struct fuse_bufvec *buf, off_t off,
                          struct fuse_file_info *);
 
        int (*read_buf) (const char *, struct fuse_bufvec **bufp,
                         size_t size, off_t off, struct fuse_file_info *);
        int (*flock) (const char *, struct fuse_file_info *, int op);
 
        int (*fallocate) (const char *, int, off_t, off_t,
                          struct fuse_file_info *);
 
        ssize_t (*copy_file_range) (const char *path_in,
                                    struct fuse_file_info *fi_in,
                                    off_t offset_in, const char *path_out,
                                    struct fuse_file_info *fi_out,
                                    off_t offset_out, size_t size, int flags);
 
        off_t (*lseek) (const char *, off_t off, int whence, struct fuse_file_info *);
};
 
struct fuse_context {
        struct fuse *fuse;
 
        uid_t uid;
 
        gid_t gid;
 
        pid_t pid;
 
        void *private_data;
 
        mode_t umask;
};
 
/*
  int fuse_main(int argc, char *argv[], const struct fuse_operations *op,
  void *private_data);
*/
#define fuse_main(argc, argv, op, private_data)                         \
        fuse_main_real(argc, argv, op, sizeof(*(op)), private_data)
 
/* ----------------------------------------------------------- *
 * More detailed API                                           *
 * ----------------------------------------------------------- */
 
void fuse_lib_help(struct fuse_args *args);
 
#if FUSE_USE_VERSION == 30
struct fuse *fuse_new_30(struct fuse_args *args, const struct fuse_operations *op,
                         size_t op_size, void *private_data);
#define fuse_new(args, op, size, data) fuse_new_30(args, op, size, data)
#else
#if (defined(LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS))
struct fuse *fuse_new(struct fuse_args *args, const struct fuse_operations *op,
                      size_t op_size, void *private_data);
#else /* LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS */
struct fuse *fuse_new_31(struct fuse_args *args,
                      const struct fuse_operations *op,
                      size_t op_size, void *private_data);
#define fuse_new(args, op, size, data) fuse_new_31(args, op, size, data)
#endif /* LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS */
#endif
 
int fuse_mount(struct fuse *f, const char *mountpoint);
 
void fuse_unmount(struct fuse *f);
 
void fuse_destroy(struct fuse *f);
 
int fuse_loop(struct fuse *f);
 
void fuse_exit(struct fuse *f);
 
#if FUSE_USE_VERSION < 32
int fuse_loop_mt_31(struct fuse *f, int clone_fd);
#define fuse_loop_mt(f, clone_fd) fuse_loop_mt_31(f, clone_fd)
#elif FUSE_USE_VERSION < FUSE_MAKE_VERSION(3, 12)
int fuse_loop_mt_32(struct fuse *f, struct fuse_loop_config *config);
#define fuse_loop_mt(f, config) fuse_loop_mt_32(f, config)
#else
#if (defined(LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS))
int fuse_loop_mt(struct fuse *f, struct fuse_loop_config *config);
#else
#define fuse_loop_mt(f, config) fuse_loop_mt_312(f, config)
#endif /* LIBFUSE_BUILT_WITH_VERSIONED_SYMBOLS */
#endif
 
 
struct fuse_context *fuse_get_context(void);
 
int fuse_getgroups(int size, gid_t list[]);
 
int fuse_interrupted(void);
 
int fuse_invalidate_path(struct fuse *f, const char *path);
 
int fuse_main_real(int argc, char *argv[], const struct fuse_operations *op,
                   size_t op_size, void *private_data);
 
int fuse_start_cleanup_thread(struct fuse *fuse);
 
void fuse_stop_cleanup_thread(struct fuse *fuse);
 
int fuse_clean_cache(struct fuse *fuse);
 
/*
 * Stacking API
 */
 
struct fuse_fs;
 
/*
 * These functions call the relevant filesystem operation, and return
 * the result.
 *
 * If the operation is not defined, they return -ENOSYS, with the
 * exception of fuse_fs_open, fuse_fs_release, fuse_fs_opendir,
 * fuse_fs_releasedir and fuse_fs_statfs, which return 0.
 */
 
int fuse_fs_getattr(struct fuse_fs *fs, const char *path, struct stat *buf,
                    struct fuse_file_info *fi);
int fuse_fs_rename(struct fuse_fs *fs, const char *oldpath,
                   const char *newpath, unsigned int flags);
int fuse_fs_unlink(struct fuse_fs *fs, const char *path);
int fuse_fs_rmdir(struct fuse_fs *fs, const char *path);
int fuse_fs_symlink(struct fuse_fs *fs, const char *linkname,
                    const char *path);
int fuse_fs_link(struct fuse_fs *fs, const char *oldpath, const char *newpath);
int fuse_fs_release(struct fuse_fs *fs,  const char *path,
                    struct fuse_file_info *fi);
int fuse_fs_open(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi);
int fuse_fs_read(struct fuse_fs *fs, const char *path, char *buf, size_t size,
                 off_t off, struct fuse_file_info *fi);
int fuse_fs_read_buf(struct fuse_fs *fs, const char *path,
                     struct fuse_bufvec **bufp, size_t size, off_t off,
                     struct fuse_file_info *fi);
int fuse_fs_write(struct fuse_fs *fs, const char *path, const char *buf,
                  size_t size, off_t off, struct fuse_file_info *fi);
int fuse_fs_write_buf(struct fuse_fs *fs, const char *path,
                      struct fuse_bufvec *buf, off_t off,
                      struct fuse_file_info *fi);
int fuse_fs_fsync(struct fuse_fs *fs, const char *path, int datasync,
                  struct fuse_file_info *fi);
int fuse_fs_flush(struct fuse_fs *fs, const char *path,
                  struct fuse_file_info *fi);
int fuse_fs_statfs(struct fuse_fs *fs, const char *path, struct statvfs *buf);
int fuse_fs_opendir(struct fuse_fs *fs, const char *path,
                    struct fuse_file_info *fi);
int fuse_fs_readdir(struct fuse_fs *fs, const char *path, void *buf,
                    fuse_fill_dir_t filler, off_t off,
                    struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int fuse_fs_fsyncdir(struct fuse_fs *fs, const char *path, int datasync,
                     struct fuse_file_info *fi);
int fuse_fs_releasedir(struct fuse_fs *fs, const char *path,
                       struct fuse_file_info *fi);
int fuse_fs_create(struct fuse_fs *fs, const char *path, mode_t mode,
                   struct fuse_file_info *fi);
int fuse_fs_lock(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi, int cmd, struct flock *lock);
int fuse_fs_flock(struct fuse_fs *fs, const char *path,
                  struct fuse_file_info *fi, int op);
int fuse_fs_chmod(struct fuse_fs *fs, const char *path, mode_t mode,
                  struct fuse_file_info *fi);
int fuse_fs_chown(struct fuse_fs *fs, const char *path, uid_t uid, gid_t gid,
                  struct fuse_file_info *fi);
int fuse_fs_truncate(struct fuse_fs *fs, const char *path, off_t size,
                     struct fuse_file_info *fi);
int fuse_fs_utimens(struct fuse_fs *fs, const char *path,
                    const struct timespec tv[2], struct fuse_file_info *fi);
int fuse_fs_access(struct fuse_fs *fs, const char *path, int mask);
int fuse_fs_readlink(struct fuse_fs *fs, const char *path, char *buf,
                     size_t len);
int fuse_fs_mknod(struct fuse_fs *fs, const char *path, mode_t mode,
                  dev_t rdev);
int fuse_fs_mkdir(struct fuse_fs *fs, const char *path, mode_t mode);
int fuse_fs_setxattr(struct fuse_fs *fs, const char *path, const char *name,
                     const char *value, size_t size, int flags);
int fuse_fs_getxattr(struct fuse_fs *fs, const char *path, const char *name,
                     char *value, size_t size);
int fuse_fs_listxattr(struct fuse_fs *fs, const char *path, char *list,
                      size_t size);
int fuse_fs_removexattr(struct fuse_fs *fs, const char *path,
                        const char *name);
int fuse_fs_bmap(struct fuse_fs *fs, const char *path, size_t blocksize,
                 uint64_t *idx);
#if FUSE_USE_VERSION < 35
int fuse_fs_ioctl(struct fuse_fs *fs, const char *path, int cmd,
                  void *arg, struct fuse_file_info *fi, unsigned int flags,
                  void *data);
#else
int fuse_fs_ioctl(struct fuse_fs *fs, const char *path, unsigned int cmd,
                  void *arg, struct fuse_file_info *fi, unsigned int flags,
                  void *data);
#endif
int fuse_fs_poll(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi, struct fuse_pollhandle *ph,
                 unsigned *reventsp);
int fuse_fs_fallocate(struct fuse_fs *fs, const char *path, int mode,
                 off_t offset, off_t length, struct fuse_file_info *fi);
ssize_t fuse_fs_copy_file_range(struct fuse_fs *fs, const char *path_in,
                                struct fuse_file_info *fi_in, off_t off_in,
                                const char *path_out,
                                struct fuse_file_info *fi_out, off_t off_out,
                                size_t len, int flags);
off_t fuse_fs_lseek(struct fuse_fs *fs, const char *path, off_t off, int whence,
                    struct fuse_file_info *fi);
void fuse_fs_init(struct fuse_fs *fs, struct fuse_conn_info *conn,
                struct fuse_config *cfg);
void fuse_fs_destroy(struct fuse_fs *fs);
 
int fuse_notify_poll(struct fuse_pollhandle *ph);
 
struct fuse_fs *fuse_fs_new(const struct fuse_operations *op, size_t op_size,
                            void *private_data);
 
typedef struct fuse_fs *(*fuse_module_factory_t)(struct fuse_args *args,
                                                 struct fuse_fs *fs[]);
#define FUSE_REGISTER_MODULE(name_, factory_) \
        fuse_module_factory_t fuse_module_ ## name_ ## _factory = factory_
 
struct fuse_session *fuse_get_session(struct fuse *f);
 
int fuse_open_channel(const char *mountpoint, const char *options);
 
#ifdef __cplusplus
}
#endif
 
#endif /* FUSE_H_ */