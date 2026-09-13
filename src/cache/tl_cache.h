#ifndef TL_CACHE_H
#define TL_CACHE_H

#include <mutex>
#include <optional>
#include <unordered_map>
#include <thallium.hpp>

namespace tl = thallium;

template <typename K, typename V> class TLCache {
    public:
    using Key = K;
    using Value = V;
    TLCache() = default;

    virtual void put_force(Key key, Value&& value) {
        std::lock_guard guard(mtx_);
        cache_[std::move(key)] = std::move(value);
    }
    virtual void remove(const Key& key) {
        std::lock_guard guard(mtx_);
        cache_.erase(key);
    }
    virtual std::optional<Value> get(const Key& key) const {
        std::lock_guard guard(mtx_);
        const auto entry = cache_.find(key);
        return entry == cache_.end() ? std::nullopt : std::optional<Value>(entry->second);
    }
    virtual ~TLCache() = default;

    private:
    std::unordered_map<Key, Value> cache_;
    mutable tl::mutex mtx_;
};

#endif
