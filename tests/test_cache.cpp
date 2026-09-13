#include "cache/cache.h"
#include <iostream>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    Cache<std::string, int> cache;
    static_assert(std::is_same_v<decltype(cache.get("x")), std::optional<int>>, "cache get must return an owned value");
    cache.put_force("x", 7);
    auto snap=cache.snapshot();
    CHECK(snap.at("x") == 7);
    cache.clear();
    CHECK(!cache.get("x").has_value());
    CHECK(snap.at("x") == 7);
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t) threads.emplace_back([&,t]{for(int i=0;i<1000;++i) cache.put_force(std::to_string(t*1000+i),int{i});});
    for(auto& t:threads)t.join();
    CHECK(cache.size() == 4000);
    return 0;
}
