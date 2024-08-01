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
#include "../cache/cache_tables.h"
#include "../fs/constants.h"
#include "../metric/cache_event.h"
#include "../metric/operation_types.h"

class Util {
    public:
    static std::string rel_to_abs_path(const char* path);

    // NOTE: returns the new argc and argv to be passed to FUSE
    static std::vector<std::string> process_args(int argc, char* argv[]);

    // NOTE: reads entire file found by path into std::vector<std::byte>
    static std::vector<std::byte> read_ent_file(const std::string& path);

    static std::optional<std::ofstream> try_get_fstream_from_path(const char* path);

    static void log_all_metrics(const std::string& path_string);

    // NOTE: clears all caches and metics
    static void reset_fs();

    // NOTE: checks whether path_string will cause recursive passthrough operation
    static bool is_recursive_path_string(const std::string& path_string);
};

#endif // UTIL_H
