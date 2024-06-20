#ifndef CU_FUSE_OPERATIONS_H
#define CU_FUSE_OPERATIONS_H

#include <iostream>
#include <ostream>
#include <cstring>

#include "../aixlog.h"

enum class OperationFunction {
    getattr,
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

class Operations {
    public:
    static void inc_operation(OperationFunction func);
    static void inc_operation_timer(OperationFunction func, long time);
    static void inc_operation_cache_hit(OperationFunction func, bool cache_hit);

    static void reset_operation_counter() { memset(operation_counter, 0, sizeof(OperationFunction::size)); }
    static void reset_operation_timer() { memset(operation_timer, 0, sizeof(OperationFunction::size)); }
    static void reset_operation_cache_hit() { memset(operation_cache_hit, 0, sizeof(OperationFunction::size)); }
    static void reset_operation_cache_miss() { memset(operation_cache_miss, 0, sizeof(OperationFunction::size)); }

    static std::ostream& log_operation(std::ostream& os);
    static std::ostream& log_operation_time(std::ostream& os);
    static std::ostream& log_operation_cache_hit(std::ostream& os);
    static std::ostream& log_operation_cache_miss(std::ostream& os);

    private:
    static inline int operation_counter[static_cast<int>(OperationFunction::size)]{};
    static inline long operation_timer[static_cast<int>(OperationFunction::size)]{};
    static inline int operation_cache_hit[static_cast<int>(OperationFunction::size)]{};
    static inline int operation_cache_miss[static_cast<int>(OperationFunction::size)]{};
};


#endif // CU_FUSE_OPERATIONS_H
