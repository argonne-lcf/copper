#ifndef DATA_CACHE_TABLE_H
#define DATA_CACHE_TABLE_H

#include <optional>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include "../aixlog.h"
#include "cache_table.h"

class DataCacheTable final : public CacheTable<std::string, std::pair<std::vector<std::byte>, size_t>> {};

#endif // DATA_CACHE_TABLE_H
