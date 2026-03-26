#include "cu_stat.h"

std::ostream& operator<<(std::ostream& os, const CuStat& cu_stat) {
    struct stat* st = reinterpret_cast<struct stat*>(cu_stat.st_vec->data());

    os << "stat {" << std::endl;
    os << "st_dev: " << st->st_dev << "," << std::endl;
    os << "st_ino: " << st->st_ino << "," << std::endl;
    os << "st_mode: " << st->st_mode << "," << std::endl;
    os << "st_nlink: " << st->st_nlink << "," << std::endl;
    os << "st_uid: " << st->st_uid << "," << std::endl;
    os << "st_gid: " << st->st_gid << "," << std::endl;
    os << "st_rdev: " << st->st_rdev << "," << std::endl;
    os << "st_size: " << st->st_size << "," << std::endl;
    os << "st_atime: " << st->st_atime << "," << std::endl;
    os << "st_mtime: " << st->st_mtime << "," << std::endl;
    os << "st_ctime: " << st->st_ctime << "," << std::endl;
    os << "st_blksize: " << st->st_blksize << "," << std::endl;
    os << "st_blocks: " << st->st_blocks << "," << std::endl;
    os << "}";

    return os;
}
