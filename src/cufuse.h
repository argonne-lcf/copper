#ifndef CUFUSE_H
#define CUFUSE_H

#define FUSE_USE_VERSION 31

#include "../libs/include/fuse/fuse.h"

/* Function prototypes for all the functions in passthrough_fh.c */

void *cu_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
void cu_destroy(void *private_data);
int cu_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int cu_access(const char *path, int mask);
int cu_readlink(const char *path, char *buf, size_t size);
int cu_opendir(const char *path, struct fuse_file_info *fi);
int cu_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int cu_releasedir(const char *path, struct fuse_file_info *fi);
int cu_mknod(const char *path, mode_t mode, dev_t rdev);
int cu_mkdir(const char *path, mode_t mode);
int cu_unlink(const char *path);
int cu_rmdir(const char *path);
int cu_symlink(const char *from, const char *to);
int cu_rename(const char *from, const char *to, unsigned int flags);
int cu_link(const char *from, const char *to);
int cu_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
int cu_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
int cu_truncate(const char *path, off_t size, struct fuse_file_info *fi);
int cu_open(const char *path, struct fuse_file_info *fi);
int cu_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int cu_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int cu_statfs(const char *path, struct statvfs *stbuf);
int cu_release(const char *path, struct fuse_file_info *fi);
int cu_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
int cu_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int cu_getxattr(const char *path, const char *name, char *value, size_t size);
int cu_listxattr(const char *path, char *list, size_t size);
int cu_removexattr(const char *path, const char *name);
int cu_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int cu_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock);
int cu_flock(const char *path, struct fuse_file_info *fi, int op);

#endif 

