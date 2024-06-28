#include "metrics.h"

tp Metric::start_operation(const OperationFunction func) {
    Operations::inc_operation(func);
    auto start = std::chrono::high_resolution_clock::now();

    return start;
}

std::pair<std::string, tp> Metric::start_cache_operation(OperationFunction func, const char* path) {
    Operations::inc_operation(func);
    auto start = std::chrono::high_resolution_clock::now();
    auto path_string = Util::rel_to_abs_path(path);

    LOG(DEBUG) << "path string: " << path_string << std::endl;

    return std::pair{path_string, start};
}

int Metric::stop_operation(const OperationFunction func, const tp start, const int ret) {
#ifdef COLLECT_METRICS
    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(func, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
#endif
    return ret;
}

int Metric::stop_cache_operation(OperationFunction func,
OperationResult res,
std::unordered_map<std::string, OpRes>& table,
std::string& path_string,
tp start,
int ret) {
#ifdef COLLECT_METRICS
    CacheEvent::record_cache_event(table, path_string, res);

    switch(res) {
    case(OperationResult::cache_hit):
        LOG(DEBUG) << "cache hit" << std::endl;
        Operations::inc_operation_cache_hit(func, true);
        break;
    case(OperationResult::cache_miss):
        LOG(DEBUG) << "cache miss" << std::endl;
        Operations::inc_operation_cache_hit(func, false);
        break;
    case(OperationResult::neg):
        LOG(DEBUG) << "cache neg" << std::endl;
        Operations::inc_operation_cache_neg(func);
        break;
    default: LOG(ERROR) << "invalid OperationResults" << std::endl; break;
    }

    auto stop = std::chrono::high_resolution_clock::now();
    Operations::inc_operation_timer(func, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());

#endif
    return ret;
}