#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <optional>
#include <string>
#include <unordered_map>
#include <exception>
#include <stdexcept>
#include <iostream>

#include <sys/stat.h>

#include "lwlog.h"

class MDCacheTable
{
public:
    MDCacheTable() {
        md_cache = std::unordered_map<std::string, struct stat*>();
    }

    void put_force(std::string key, struct stat* st);
    std::optional<struct stat*> get(std::string);

    void log();
    void log_key_value(std::string key, struct stat* st);
    void log_value(struct stat*);
private:
    std::unordered_map<std::string, struct stat*> md_cache;
};

#endif // MDCACHETABLE_H
