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
    static std::vector<std::string> process_args(int argc, const char* argv[]);

    // NOTE: reads entire file found by path into std::vector<std::byte>
    static std::vector<std::byte> read_ent_file(const std::string& path, bool is_file);

    static std::optional<std::ofstream> try_get_fstream_from_path(const char* path);

    static std::string get_current_datetime();
};

#endif // UTIL_H
