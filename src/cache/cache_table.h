#ifndef CACHE_TABLE_H
#define CACHE_TABLE_H

#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>

template <typename K, typename V> class CacheTable {
    public:
    using Key = K;
    using Value = V;

    CacheTable() {
        cache = std::unordered_map<Key, Value>();
    }

    virtual void put_force(Key key, Value val) {
        cache[key] = val;
    };

    virtual std::optional<Value> get(Key key) {
        try {
            return cache.at(key);
        } catch(std::out_of_range& e) {
            return std::nullopt;
        }
    };

    template <typename K_, typename V_>
    friend std::ostream& operator<<(std::ostream& os, const CacheTable<K_, V_>& cache_table);

    virtual ~CacheTable() = default;

    private:
    std::unordered_map<Key, Value> cache;
};

#endif // CACHE_TABLE_H
