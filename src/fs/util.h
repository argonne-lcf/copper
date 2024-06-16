#ifndef UTIL_H
#define UTIL_H

#define FUSE_USE_VERSION 31
#include <exception>
#include <fuse.h>
#include <string>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <algorithm>

#include "../aixlog.h"
#include "constants.h"
#include "cu_stat.h"
#include "../cache/cur_cache.h"

class Util {
    public:
    static std::string rel_to_abs_path(const char* path);
    static std::string get_base_of_path(const std::string& str);
    static char* deep_cpy_string(const std::string& str);
    static int gen_inode();

    // NOTE: returns the new argc and argv to be passed to FUSE
    static std::vector<std::string> process_args(int argc, const char* argv[]);

    // NOTE: adds the target dir to the cache on cu_fuse_init
    static void cache_target_path();

    // NOTE: reads entire file found by path into std::vector<std::byte>
    static std::vector<std::byte> read_ent_file(const std::string &path, bool is_file);

    // NOTE: removes path from all caches
    static void remove_entry_from_cache(std::string path);

    // NOTE: just checks if a data entry exists
    static bool is_dir(std::string path);
};

#endif // UTIL_H
