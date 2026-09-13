#include "cache/path_status_cache.h"
#include <atomic>
#include <cerrno>
#include <iostream>
#include <thread>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    PathStatusCache cache;
    auto leader = cache.begin("same");
    CHECK(leader.is_leader());
    auto joined = cache.begin("same");
    CHECK(!joined.is_leader());

    std::atomic<bool> waiter_has_token{false};
    std::atomic<bool> allow_wait{false};
    int joined_result = 0;
    std::thread waiter([&] {
        waiter_has_token = true;
        while(!allow_wait.load()) std::this_thread::yield();
        joined_result = cache.wait(joined);
    });
    while(!waiter_has_token.load()) std::this_thread::yield();

    cache.complete(leader, -ENOENT);
    auto next = cache.begin("same");
    CHECK(next.is_leader());
    cache.complete(next, 17);
    allow_wait = true;
    waiter.join();
    CHECK(joined_result == -ENOENT);
    CHECK(cache.wait(next) == 17);
    CHECK(cache.active_size() == 0);

    for(int i=0; i<1000; ++i) {
        auto request = cache.begin(std::to_string(i));
        cache.complete(request, i);
    }
    CHECK(cache.active_size() == 0);
    return 0;
}