#ifndef CU_FUSE_OPERATIONS_H
#define CU_FUSE_OPERATIONS_H

#include <cstring>
#include <iostream>
#include <ostream>

#include "../fs/util.h"
#include "operation_types.h"

class Operations {
    public:
    static void inc_operation(OperationFunction func);
    static void inc_operation_timer(OperationFunction func, long time);
    static void inc_operation_cache_hit(OperationFunction func, bool cache_hit);
    static void inc_operation_neg(OperationFunction func);

    static void reset_operation_counter() {
        memset(operation_counter, 0, sizeof(OperationFunction::size));
    }
    static void reset_operation_timer() {
        memset(operation_timer, 0, sizeof(OperationFunction::size));
    }
    static void reset_operation_cache_hit() {
        memset(operation_cache_hit, 0, sizeof(OperationFunction::size));
    }
    static void reset_operation_cache_miss() {
        memset(operation_cache_miss, 0, sizeof(OperationFunction::size));
    }
    static void reset_operation_neg() {
        memset(operation_neg, 0, sizeof(OperationFunction::size));
    }

    static std::ostream& log_operation(std::ostream& os);
    static std::ostream& log_operation_time(std::ostream& os);
    static std::ostream& log_operation_cache_hit(std::ostream& os);
    static std::ostream& log_operation_cache_miss(std::ostream& os);
    static std::ostream& log_operation_neg(std::ostream& os);

    private:
    static inline int operation_counter[static_cast<int>(OperationFunction::size)]{};
    static inline long operation_timer[static_cast<long>(OperationFunction::size)]{};
    static inline int operation_cache_hit[static_cast<int>(OperationFunction::size)]{};
    static inline int operation_cache_miss[static_cast<int>(OperationFunction::size)]{};
    static inline int operation_neg[static_cast<int>(OperationFunction::size)]{};
};


#endif // CU_FUSE_OPERATIONS_H
