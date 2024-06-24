#ifndef UTIL_H
#define UTIL_H

#define FUSE_USE_VERSION 31
#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fuse.h>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <sys/stat.h>

#include "../aixlog.h"
#include "../cache/cur_cache.h"
#include "../fs/constants.h"
#include "../metric/cache_event.h"
#include "../metric/operation_types.h"

using tp = std::chrono::time_point<std::chrono::system_clock>;

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
    static std::vector<std::byte> read_ent_file(const std::string& path, bool is_file);

    // NOTE: removes path from all caches
    static void remove_entry_from_cache(std::string path);

    // NOTE: just checks if a data entry exists
    static bool is_dir(std::string path);

    static std::optional<std::ofstream> try_get_fstream_from_path(const char* path);

    static std::string get_current_datetime();

    static tp start_operation(OperationFunction func);
    static int stop_operation(OperationFunction func, tp start, int ret);

    static std::pair<std::string, tp> start_cache_operation(OperationFunction func, const char* path);
    static int stop_cache_operation(OperationFunction func,
    OperationResult res,
    std::unordered_map<std::string, OpRes>& table,
    std::string& path_string,
    tp start,
    int ret);
};

#endif // UTIL_H
