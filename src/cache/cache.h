#ifndef CACH_H
#define CACH_H

#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

template <typename K, typename V> class Cache {
    public:
    using Key = K;
    using Value = V;

    Cache() = default;

    virtual void put_force(Key key, Value&& val) {
        std::lock_guard guard(mtx);
        cache[key] = std::move(val);
    };

    virtual void remove(Key key) {
        std::lock_guard guard(mtx);
        cache.erase(key);
    }

    template <typename Function> bool update(const Key& key, Function&& function) {
        std::lock_guard guard(mtx);
        auto entry = cache.find(key);
        if(entry == cache.end()) return false;
        std::forward<Function>(function)(entry->second);
        return true;
    }

    void clear() {
        std::lock_guard guard(mtx);
        cache.clear();
    }

    std::unordered_map<Key, Value> snapshot() const {
        std::lock_guard guard(mtx);
        return cache;
    }

    std::size_t size() const {
        std::lock_guard guard(mtx);
        return cache.size();
    }

    virtual std::optional<Value> get(const Key& key) const {
        std::lock_guard guard(mtx);
        const auto entry = cache.find(key);
        if(entry == cache.end()) return std::nullopt;
        return entry->second;
    };

    virtual ~Cache() = default;
    private:
    std::unordered_map<Key, Value> cache;
    mutable std::mutex mtx;
};

#endif // CACHE_H
