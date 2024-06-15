#ifndef UTIL_H
#define UTIL_H

#define FUSE_USE_VERSION 31
#include <exception>
#include <fuse.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

#include "../aixlog.h"
#include "constants.h"

class Util {
    public:
    static std::string rel_to_abs_path(const char* path);

    // NOTE: returns the new argc and argv to be passed to FUSE
    static std::vector<std::string> process_args(int argc, const char* argv[]);
};

#endif // UTIL_H
