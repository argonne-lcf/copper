#ifndef CU_FUSE_OPERATION_TYPES_H
#define CU_FUSE_OPERATION_TYPES_H

struct OpRes {
    unsigned int cache_hit = 0;
    unsigned int cache_miss = 0;
    unsigned int neg = 0;
};

enum class OperationResult { cache_hit, cache_miss, neg };

enum class OperationFunction {
    getattr = 0,
    readlink,
    mknod,
    mkdir,
    unlink,
    rmdir,
    symlink,
    rename,
    link,
    chmod,
    chown,
    truncate,
    open,
    read,
    write,
    statfs,
    flush,
    release,
    fsync,
    setxattr,
    getxattr,
    listxattr,
    removexattr,
    opendir,
    readdir,
    releasedir,
    fsyncdir,
    init,
    destroy,
    access,
    create,
    lock,
    utimens,
    bmap,
    ioctl,
    poll,
    write_buf,
    read_buf,
    flock,
    fallocate,
    copy_file_range,
    lseek,
    size,
};

#endif // CU_FUSE_OPERATION_TYPES_H
