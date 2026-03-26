#ifndef CU_STAT_H
#define CU_STAT_H

#include <cstring>
#include <memory>
#include <ostream>
#include <sys/stat.h>
#include <vector>

class CuStat {
    public:
    using stat_type = std::vector<std::byte>;

    CuStat() : st_vec(std::make_shared<stat_type>(sizeof(struct stat))) {
    }

    explicit CuStat(const stat_type& st) : st_vec(std::make_shared<stat_type>(st)) {
    }

    CuStat(const CuStat& other) : st_vec(std::make_shared<stat_type>(*other.st_vec)) {
    }

    CuStat& operator=(const CuStat& other) {
        if(this != &other) {
            st_vec = std::make_shared<stat_type>(*other.st_vec);
        }
        return *this;
    }

    CuStat(CuStat&& other) noexcept : st_vec(std::move(other.st_vec)) {
    }

    CuStat& operator=(CuStat&& other) noexcept {
        if(this != &other) {
            st_vec = std::move(other.st_vec);
        }
        return *this;
    }

    [[nodiscard]] struct stat* get_st() const {
        return reinterpret_cast<struct stat*>(st_vec->data());
    }

    void cp_to_buf(struct stat* buf) const {
        std::memcpy(buf, st_vec->data(), sizeof(struct stat));
    }

    const stat_type& get_vec() {
        return *st_vec;
    }

    friend std::ostream& operator<<(std::ostream& os, const CuStat& cu_stat);

    private:
    std::shared_ptr<stat_type> st_vec;
};

#endif // CU_STAT_H
