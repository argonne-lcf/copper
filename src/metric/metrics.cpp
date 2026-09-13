#include "metrics.h"

namespace {
ProfileOutcome profile_outcome(OperationResult result) {
    switch(result) {
    case OperationResult::success: return ProfileOutcome::success;
    case OperationResult::cache_hit: return ProfileOutcome::cache_hit;
    case OperationResult::cache_miss: return ProfileOutcome::cache_miss;
    case OperationResult::neg: return ProfileOutcome::negative;
    }
    return ProfileOutcome::negative;
}
} // namespace

tp Metric::start_operation(const OperationFunction) {
    return std::chrono::steady_clock::now();
}

std::pair<std::string, tp> Metric::start_cache_operation(OperationFunction, const char* path) {
    auto start = std::chrono::steady_clock::now();
    auto path_string = Util::rel_to_abs_path(path);
    LOG(DEBUG) << "path string: " << path_string << std::endl;
    return std::pair{path_string, start};
}

int Metric::stop_operation(const OperationFunction func, const tp start, const int ret) {
    if(!Operations::enabled()) return ret;
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
    Operations::complete(func, ns, ret < 0 ? ProfileOutcome::negative : ProfileOutcome::success,
                         ret < 0 ? -ret : 0);
    return ret;
}

int Metric::stop_cache_operation(OperationFunction func, OperationResult result, ProfileSource source,
                                 std::string& path_string, tp start, int ret) {
    if(!Operations::enabled()) return ret;
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
    Operations::complete(func, ns, profile_outcome(result), ret < 0 ? -ret : 0, path_string, source);
    return ret;
}
