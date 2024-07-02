#ifndef CU_FUSE_H
#define CU_FUSE_H

#include <stdexcept>
#define FUSE_USE_VERSION 31
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fuse.h>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <variant>
#include <vector>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <margo.h>
#include <pthread.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

#include "../aixlog.h"
#include "../cache/cache_tables.h"
#include "constants.h"
#include "util.h"
#include "../metric/cache_event.h"
#include "../metric/metrics.h"
#include "../metric/operations.h"
#include "../metric/ioctl_event.h"

class CuFuse {
    public:
    static int cu_hello_main(int argc, char* argv[], void* userdata);
};

#endif //CU_FUSE_H
