#ifndef STAT_H
#define STAT_H

#include <cstring>
#include <ostream>
#include <sys/stat.h>
#include "../aixlog.h"

class CuStat {
    public:
    CuStat() : st{new struct stat} {}

    explicit CuStat(const struct stat* _st) : st{new struct stat} {
        std::memcpy(st, _st, sizeof(struct stat));
    }

    CuStat(const CuStat& other) : st{new struct stat} {
        std::memcpy(st, other.get_st(), sizeof(struct stat));
    }

    CuStat& operator=(const CuStat& other) {
        if(this != &other) {
            delete st;
            const auto temp = new struct stat;
            std::memcpy(temp, other.get_st(), sizeof(struct stat));
            st = temp;
        }

        return *this;
    }

    CuStat(CuStat&& other) noexcept : st{other.get_st()} {
        other.st = nullptr;
    }

    CuStat& operator=(CuStat&& other) noexcept {
        if(this != &other) {
            delete st;
            st = other.get_st();
            other.st = nullptr;
        }

        return *this;
    }

    struct stat* get_st() const { return st; }
    struct stat* get_st_cpy() const {
        auto cpy_st = new struct stat;
        memcpy(cpy_st, st, sizeof(struct stat));

        return cpy_st;
    }

    void cp_to_buf(struct stat* buf) const;
    friend std::ostream& operator<<(std::ostream& os, const CuStat& cu_stat);

    ~CuStat() {
        delete st;
    }

    private:
    struct stat* st = nullptr;
};

#endif // STAT_H
