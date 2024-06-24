#ifndef METRICS_H
#define METRICS_H

#define COLLECT_METRICS 1

#include <utility>
#include <unordered_map>
#include <string>

#include "operations.h"
#include "operation_types.h"
#include "../aixlog.h"
#include "../fs/util.h"

class Metric {
    public:
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

#endif // METRICS_H
