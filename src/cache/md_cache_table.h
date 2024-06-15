#ifndef MDCACHETABLE_H
#define MDCACHETABLE_H

#include <string>
#include <sys/stat.h>

#include "../fs/cu_stat.h"
#include "cache_table.h"

class MDCacheTable final : public CacheTable<std::string, CuStat> {};

#endif // MDCACHETABLE_H
