#ifndef METRICS_H
#define METRICS_H

#define COLLECT_METRICS

#include <string>
#include <unordered_map>
#include <utility>

#include "../aixlog.h"
#include "../fs/util.h"
#include "operation_types.h"
#include "operations.h"

class Metric {
    public:
    static tp start_operation(OperationFunction func);
    static int stop_operation(OperationFunction func, tp start, int ret);

    static inline int stat_inter_cache_hit = 0;
    static inline int stat_inter_cache_miss = 0;

    static inline int read_inter_cache_hit = 0;
    static inline int read_inter_cache_miss = 0;

    static inline int readdir_inter_cache_hit = 0;
    static inline int readdir_inter_cache_miss = 0;

    static std::pair<std::string, tp> start_cache_operation(OperationFunction func, const char* path);
    static int stop_cache_operation(OperationFunction func,
    OperationResult res,
    std::unordered_map<std::string, OpRes>& table,
    std::string& path_string,
    tp start,
    int ret);
};

#endif // METRICS_H
