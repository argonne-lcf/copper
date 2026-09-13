#ifndef PATH_STATUS_CACHE_H
#define PATH_STATUS_CACHE_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <stddef.h>

class PathStatusCache final {
    class State;
    public:
    using Key = std::string;
    class Request {
    public:
        bool is_leader() const { return leader_; }
    private:
        Request(Key key, std::shared_ptr<State> state, bool leader)
            : key_(std::move(key)), state_(std::move(state)), leader_(leader) {}
        Key key_;
        std::shared_ptr<State> state_;
        bool leader_ = false;
        friend class PathStatusCache;
    };

    Request begin(const Key& key);
    void complete(const Request& request, ssize_t status);
    int wait(const Request& request);
    size_t active_size() const;


    private:
    class State {
    public:
        bool complete = false;
        int status = 0;
    };
    std::unordered_map<Key, std::shared_ptr<State>> active_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

#endif
