#ifndef CACHE_TABLE_H
#define CACHE_TABLE_H

#include <optional>

template <typename K, typename V> class CacheTable {
    public:
    using Key   = K;
    using Value = V;

    virtual void put_force(Key key, Value val) = 0;
    virtual std::optional<Value> get(Key key)  = 0;

    virtual void log()                             = 0;
    virtual void log_key_value(Key key, Value val) = 0;
    virtual void log_value(Value val)              = 0;

    virtual ~CacheTable() {
    }
};

#endif // CACHE_TABLE_H
