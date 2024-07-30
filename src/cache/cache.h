#ifndef CACHE_H
#define CACHE_H

#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <thallium.hpp>

namespace tl = thallium;

template <typename K, typename V> class Cache {
    public:
    using Key = K;
    using Value = V;

    Cache() {
        std::lock_guard guard(mtx);
        cache = std::unordered_map<Key, Value>();
    }

    virtual void put_force(Key key, Value&& val) {
        std::lock_guard guard(mtx);
        cache[key] = std::move(val);
    };

    virtual void remove(Key key) {
        std::lock_guard guard(mtx);
        cache.erase(key);
    }

    virtual std::optional<Value*> get(Key key) {
        std::lock_guard guard(mtx);
        try {
            return &cache.at(key);
        } catch(std::out_of_range& e) {
            return std::nullopt;
        }
    };

    virtual ~Cache() = default;
    std::unordered_map<Key, Value> cache;

    tl::mutex mtx;
};

#endif // CACHE_H
