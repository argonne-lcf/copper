#include "cache_table.h"

#define MAX_ELEMENTS_TO_PRINT 10

template <typename K_, typename V_> std::ostream& operator<<(std::ostream& os, const CacheTable<K_, V_>& cache_table) {
    int cur_elements = 1;

    os << "cache {" << std::endl;

    for(auto const& pair : cache_table) {
        os << pair.first << " {" << std::endl;
        os << std::endl << pair.second << std::endl;
        os << "}" << std::endl;

        if(cur_elements++ >= MAX_ELEMENTS_TO_PRINT) {
            os << "..." << std::endl;
        }
    }

    os << "}";

    return os;
}