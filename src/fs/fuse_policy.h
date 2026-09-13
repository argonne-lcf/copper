#ifndef COPPER_FUSE_POLICY_H
#define COPPER_FUSE_POLICY_H
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

namespace FusePolicy {
inline int mutation_result() { return -EROFS; }
inline int optional_result() { return -ENOSYS; }
inline int compatibility_result() { return 0; }
inline int open_result(int flags) {
    return ((flags & O_ACCMODE) == O_RDONLY && !(flags & (O_CREAT | O_TRUNC | O_APPEND))) ? 0 : -EROFS;
}
inline int access_result(int mask, int lookup_errno) {
    if(mask & W_OK) return -EROFS;
    return lookup_errno != 0 ? -lookup_errno : 0;
}
inline int read_offset_result(off_t offset) {
    return offset < 0 ? -EINVAL : 0;
}
inline off_t lseek_result(off_t size, off_t off, int whence) {
    if(size < 0) {
        return -EINVAL;
    }
    off_t value = 0;
    if(whence == SEEK_SET) {
        value = off;
    } else if(whence == SEEK_CUR) {
        return -ENOSYS;
    } else if(whence == SEEK_END) {
        if(off > 0 && size > std::numeric_limits<off_t>::max() - off) return -EOVERFLOW;
        if(off < 0 && off < -size) return -EINVAL;
        value = size + off;
    } else {
        return -EINVAL;
    }
    return value < 0 ? static_cast<off_t>(-EINVAL) : value;
}
} // namespace FusePolicy
#endif
