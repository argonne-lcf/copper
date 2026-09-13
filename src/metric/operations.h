#ifndef CU_FUSE_OPERATIONS_H
#define CU_FUSE_OPERATIONS_H

#include <cstdint>
#include <ostream>

#include "operation_types.h"
#include "profiler_core.h"

class Operations {
    public:
    static void complete(OperationFunction func, uint64_t nanoseconds, ProfileOutcome outcome,
                         int error = 0, const std::string& path = "",
                         ProfileSource source = ProfileSource::none);
    static ProfilerSnapshot snapshot();
    static ProfilerSnapshot snapshot_and_reset();
    static ProfilerConfig config();
    static bool enabled();
    static ProfilerCore& core();

    // Compatibility views now read one coherent snapshot from the same collector.
    static int get_operation_count(OperationFunction func);
    static uint64_t get_operation_time(OperationFunction func);
    static int get_operation_cache_hit(OperationFunction func);
    static int get_operation_cache_miss(OperationFunction func);
    static int get_operation_cache_neg(OperationFunction func);
    static void inc_operation(OperationFunction func);
    static void reset_operation_counter();
    static void reset_operation_timer();
    static void reset_operation_cache_hit();
    static void reset_operation_cache_miss();
    static void reset_operation_cache_neg();

    static std::ostream& log_operation(std::ostream& os);
    static std::ostream& log_operation_time(std::ostream& os);
    static std::ostream& log_operation_cache_hit(std::ostream& os);
    static std::ostream& log_operation_cache_miss(std::ostream& os);
    static std::ostream& log_operation_neg(std::ostream& os);
};

#endif
