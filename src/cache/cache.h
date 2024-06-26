#ifndef CACHE_TABLE_H
#define CACHE_TABLE_H

#include <optional>
#include <stdexcept>
#include <unordered_map>

template <typename K, typename V> class Cache {
    public:
    using Key = K;
    using Value = V;

    Cache() {
        cache = std::unordered_map<Key, Value>();
    }

    virtual void put_force(Key key, Value&& val) {
        cache[key] = std::move(val);
    };

    virtual void remove(Key key) {
        cache.erase(key);
    }

    virtual std::optional<Value*> get(Key key) {
        try {
            return &cache.at(key);
        } catch(std::out_of_range& e) {
            return std::nullopt;
        }
    };

    virtual ~Cache() = default;
    std::unordered_map<Key, Value> cache;
};

#endif // CACHE_TABLE_H
