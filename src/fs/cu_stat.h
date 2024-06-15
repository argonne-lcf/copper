#ifndef STAT_H
#define STAT_H

#include <cstring>
#include <ostream>
#include <sys/stat.h>

class CuStat {
    public:
    CuStat() {
        st = new struct stat;
    }

    void cp_to_buf(struct stat* buf) const;
    friend std::ostream& operator<<(std::ostream& os, const CuStat& cu_stat);

    ~CuStat() {
        delete st;
    }

    private:
    struct stat* st;
};

#endif // STAT_H
