#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <sys/stat.h>

#include "aixlog.h"
#include "cache_table.h"

class MDCacheTable : public CacheTable<std::string, struct stat*> {
    public:
    MDCacheTable() {
        md_cache = std::unordered_map<Key, Value>();
    }

    void put_force(Key path, Value st) override;
    std::optional<MDCacheTable::Value> get(Key path) override;

    void log() override;
    void log_key_value(Key path, Value st) override;
    void log_value(Value st) override;

    private:
    std::unordered_map<Key, Value> md_cache;
};

#endif // MDCACHETABLE_H
