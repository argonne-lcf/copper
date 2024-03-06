// passthrough_fh.h

#ifndef PASSTHROUGH_FH_H
#define PASSTHROUGH_FH_H

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Function declarations
int passthrough_fh_getattr(const char *path, struct stat *stbuf);
int passthrough_fh_readlink(const char *path, char *buf, size_t size);
int passthrough_fh_mknod(const char *path, mode_t mode, dev_t rdev);
int passthrough_fh_mkdir(const char *path, mode_t mode);
int passthrough_fh_unlink(const char *path);
int passthrough_fh_rmdir(const char *path);
int passthrough_fh_symlink(const char *from, const char *to);
int passthrough_fh_rename(const char *from, const char *to, unsigned int flags);
int passthrough_fh_link(const char *from, const char *to);
int passthrough_fh_chmod(const char *path, mode_t mode);
int passthrough_fh_chown(const char *path, uid_t uid, gid_t gid);
int passthrough_fh_truncate(const char *path, off_t size);
int passthrough_fh_open(const char *path, struct fuse_file_info *fi);
int passthrough_fh_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int passthrough_fh_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int passthrough_fh_statfs(const char *path, struct statvfs *stbuf);
int passthrough_fh_flush(const char *path, struct fuse_file_info *fi);
int passthrough_fh_release(const char *path, struct fuse_file_info *fi);
int passthrough_fh_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
int passthrough_fh_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int passthrough_fh_getxattr(const char *path, const char *name, char *value, size_t size);
int passthrough_fh_listxattr(const char *path, char *list, size_t size);
int passthrough_fh_removexattr(const char *path, const char *name);
int passthrough_fh_opendir(const char *path, struct fuse_file_info *fi);
int passthrough_fh_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int passthrough_fh_releasedir(const char *path, struct fuse_file_info *fi);
int passthrough_fh_fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi);
void passthrough_fh_destroy(void *private_data);
int passthrough_fh_access(const char *path, int mask);
int passthrough_fh_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int passthrough_fh_ftruncate(const char *path, off_t size, struct fuse_file_info *fi);
int passthrough_fh_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int passthrough_fh_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock);
int passthrough_fh_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int passthrough_fh_bmap(const char *path, size_t blocksize, uint64_t *idx);
int passthrough_fh_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi, unsigned int flags, void *data);
int passthrough_fh_poll(const char *path, struct fuse_file_info *fi, struct fuse_pollhandle *ph, unsigned *reventsp);
int passthrough_fh_write_buf(const char *path, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *fi);
int passthrough_fh_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *fi);
int passthrough_fh_flock(const char *path, struct fuse_file_info *fi, int op);
int passthrough_fh_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *fi);

// Main function (entry point for FUSE)
int passthrough_fh_main(int argc, char *argv[]);

#endif // PASSTHROUGH
