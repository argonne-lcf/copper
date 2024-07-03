#include "operations.h"

template <typename t>
static void
log_operation_helper(const std::string& table_name, std::ostream& os, t (&table_array)[static_cast<int>(OperationFunction::size)]) {
    std::vector<std::pair<int, t>> indexed_arr;
    indexed_arr.reserve(static_cast<int>(OperationFunction::size));
    for(int i = 0; i < static_cast<int>(OperationFunction::size); ++i) {
        indexed_arr.push_back(std::make_pair(table_array[i], i));
    }

    std::sort(indexed_arr.begin(), indexed_arr.end(), std::greater<>());

    os << table_name << " {" << std::endl;
    for(size_t i = 0; i < indexed_arr.size(); ++i) {
        const auto& pair = indexed_arr[i];
        os << OperationFunctionNames[pair.second] << ": " << table_array[pair.second];
        if(i != indexed_arr.size() - 1) {
            os << ",";
        }
        os << std::endl;
    }
    os << "}";
}

void Operations::inc_operation(OperationFunction func) {
    operation_counter[static_cast<int>(func)]++;
}

void Operations::inc_operation_cache_neg(OperationFunction func) {
    operation_cache_neg[static_cast<int>(func)]++;
}

void Operations::inc_operation_timer(OperationFunction func, const long time) {
    operation_timer[static_cast<long>(func)] += time;
}

void Operations::inc_operation_cache_hit(OperationFunction func, const bool cache_hit) {
    if(cache_hit) {
        operation_cache_hit[static_cast<int>(func)]++;
    } else {
        operation_cache_miss[static_cast<int>(func)]++;
    }
}

std::ostream& Operations::log_operation_neg(std::ostream& os) {
    log_operation_helper<int>("Aggregate Operation Cache Negative", os, operation_cache_neg);
    return os;
}

std::ostream& Operations::log_operation(std::ostream& os) {
    log_operation_helper<int>("Aggregate Operation Counter", os, operation_counter);
    return os;
}

std::ostream& Operations::log_operation_time(std::ostream& os) {
    log_operation_helper<long>("Aggregate Operation Time (microseconds)", os, operation_timer);
    return os;
}

std::ostream& Operations::log_operation_cache_hit(std::ostream& os) {
    log_operation_helper<int>("Aggregate Operation Cache Hit", os, operation_cache_hit);
    return os;
}

std::ostream& Operations::log_operation_cache_miss(std::ostream& os) {
    log_operation_helper<int>("Aggregate Operation Cache Miss", os, operation_cache_miss);
    return os;
}