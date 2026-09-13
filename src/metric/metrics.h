#ifndef METRICS_H
#define METRICS_H

#include <string>

#include <utility>

#include "../aixlog.h"
#include "../fs/util.h"
#include "operation_types.h"
#include "operations.h"

class Metric {
    public:
    static tp start_operation(OperationFunction func);
    static int stop_operation(OperationFunction func, tp start, int ret);

    static std::pair<std::string, tp> start_cache_operation(OperationFunction func, const char* path);
    static int stop_cache_operation(OperationFunction func,
    OperationResult res,
    ProfileSource source,
    std::string& path_string,
    tp start,
    int ret);
};

#endif // METRICS_H
