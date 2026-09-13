#include "fs/fuse_policy.h"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <unistd.h>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    using namespace FusePolicy;
    CHECK(mutation_result() == -EROFS);
    CHECK(optional_result() == -ENOSYS);
    CHECK(open_result(O_RDONLY) == 0);
    CHECK(open_result(O_WRONLY) == -EROFS);
    CHECK(open_result(O_RDWR) == -EROFS);
    CHECK(compatibility_result() == 0);
    CHECK(access_result(R_OK, 0) == 0);
    CHECK(access_result(R_OK, EACCES) == -EACCES);
    CHECK(access_result(W_OK, 0) == -EROFS);
    CHECK(access_result(W_OK, ENOENT) == -EROFS);
    CHECK(access_result(F_OK, ENOENT) == -ENOENT);
    CHECK(lseek_result(10, 3, SEEK_SET) == 3);
    CHECK(lseek_result(10, -1, SEEK_SET) == -EINVAL);
    CHECK(lseek_result(10, 0, SEEK_END) == 10);
    CHECK(lseek_result(10, 1, SEEK_END) == 11);
    CHECK(lseek_result(10, -11, SEEK_END) == -EINVAL);
    CHECK(lseek_result(std::numeric_limits<off_t>::max(), 1, SEEK_END) == -EOVERFLOW);
    CHECK(lseek_result(0, std::numeric_limits<off_t>::min(), SEEK_END) == -EINVAL);
    CHECK(lseek_result(std::numeric_limits<off_t>::max(), std::numeric_limits<off_t>::min(), SEEK_END) == -EINVAL);
    CHECK(lseek_result(1, std::numeric_limits<off_t>::max(), SEEK_END) == -EOVERFLOW);
    CHECK(lseek_result(10, 3, SEEK_CUR) == -ENOSYS);
    CHECK(read_offset_result(-1) == -EINVAL);
    CHECK(read_offset_result(std::numeric_limits<off_t>::min()) == -EINVAL);
    CHECK(read_offset_result(0) == 0);
    CHECK(read_offset_result(std::numeric_limits<off_t>::max()) == 0);
    return 0;
}
