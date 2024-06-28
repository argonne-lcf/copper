#include "cu_stat.h"

void CuStat::cp_to_buf(struct stat* buf) const {
    memcpy(buf, st, sizeof(struct stat));
}

std::ostream& operator<<(std::ostream& os, const CuStat& cu_stat) {
    os << "stat {" << std::endl;
    os << "st_dev: " << cu_stat.st->st_dev << "," << std::endl;
    os << "st_ino: " << cu_stat.st->st_ino << "," << std::endl;
    os << "st_mode: " << cu_stat.st->st_mode << "," << std::endl;
    os << "st_nlink: " << cu_stat.st->st_nlink << "," << std::endl;
    os << "st_uid: " << cu_stat.st->st_uid << "," << std::endl;
    os << "st_gid: " << cu_stat.st->st_gid << "," << std::endl;
    os << "st_rdev: " << cu_stat.st->st_rdev << "," << std::endl;
    os << "st_size: " << cu_stat.st->st_size << "," << std::endl;
    os << "st_atime: " << cu_stat.st->st_atime << "," << std::endl;
    os << "st_mtime: " << cu_stat.st->st_mtime << "," << std::endl;
    os << "st_ctime: " << cu_stat.st->st_ctime << "," << std::endl;
    os << "st_blksize: " << cu_stat.st->st_blksize << "," << std::endl;
    os << "st_blocks: " << cu_stat.st->st_blocks << "," << std::endl;
    os << "}";

    return os;
}
