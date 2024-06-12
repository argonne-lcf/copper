#ifndef DATA_CACHE_TABLE_H
#define DATA_CACHE_TABLE_H

#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>

#include "aixlog.h"
#include "cache_table.h"

class DataCacheTable : public CacheTable<std::string, std::pair<std::vector<std::byte>, ino_t>> {
    public:
    DataCacheTable() {
        data_cache = std::unordered_map<Key, Value>();
    }

    void put_force(Key path, Value data_inode) override;
    std::optional<Value> get(Key path) override;

    void log() override;
    void log_key_value(Key path, Value data_inode) override;
    void log_value(Value data_inode) override;

    private:
    std::unordered_map<Key, Value> data_cache;
};

#endif // DATA_CACHE_TABLE_H
