#include "path_status_cache.h"

PathStatusCache::Request PathStatusCache::begin(const Key& key) {
    std::lock_guard guard(mutex_);
    auto entry = active_.find(key);
    if(entry != active_.end()) {
        return Request(key, entry->second, false);
    }
    auto state = std::make_shared<State>();
    active_.emplace(key, state);
    return Request(key, std::move(state), true);
}

void PathStatusCache::complete(const Request& request, const ssize_t status) {
    std::lock_guard guard(mutex_);
    if(!request.state_ || request.state_->complete) return;
    request.state_->status = static_cast<int>(status);
    request.state_->complete = true;
    const auto entry = active_.find(request.key_);
    if(entry != active_.end() && entry->second == request.state_) {
        active_.erase(entry);
    }
    cv_.notify_all();
}

int PathStatusCache::wait(const Request& request) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return request.state_ && request.state_->complete; });
    return request.state_->status;
}

size_t PathStatusCache::active_size() const {
    std::lock_guard guard(mutex_);
    return active_.size();
}
