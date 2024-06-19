#ifndef CU_FUSE_OPERATIONS_H
#define CU_FUSE_OPERATIONS_H

#include <iostream>
#include <ostream>

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

    static std::ostream& log_operations(std::ostream& os);
    static std::ostream& log_operations_time(std::ostream& os);

    private:
    static inline int operation_counter[static_cast<int>(OperationFunction::size)]{};
    static inline int operations_timer[static_cast<int>(OperationFunction::size)];
};


#endif // CU_FUSE_OPERATIONS_H
