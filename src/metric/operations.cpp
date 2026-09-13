#include "operations.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <vector>

#include "../fs/constants.h"

namespace {
size_t latency_capacity() {
    constexpr size_t fallback = 1024;
    constexpr size_t maximum = 65536;
    const char* value = std::getenv("COPPER_PROFILE_LATENCY_SAMPLES");
    if(!value) return fallback;
    const std::string_view text(value);
    if(text.empty()) return fallback;
    size_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if(result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed > maximum) {
        return fallback;
    }
    return parsed;
}

uint64_t outcome(const OperationSnapshot& op, ProfileOutcome value) {
    return op.outcomes[static_cast<size_t>(value)];
}

template <typename Value>
std::ostream& log_snapshot(const std::string& name, std::ostream& os,
                           const ProfilerSnapshot& snapshot, Value value) {
    std::vector<std::pair<uint64_t, size_t>> entries;
    for(size_t i = 0; i < snapshot.operations.size(); ++i) entries.emplace_back(value(snapshot.operations[i]), i);
    std::sort(entries.begin(), entries.end(), std::greater<>());
    os << name << " {\n";
    for(size_t i = 0; i < entries.size(); ++i) {
        os << OperationFunctionNames[entries[i].second] << ": " << entries[i].first;
        if(i + 1 != entries.size()) os << ',';
        os << '\n';
    }
    return os << '}';
}
} // namespace

ProfilerConfig Operations::config() {
    return {static_cast<size_t>(std::max(0, Constants::profile_top_n)), Constants::profile_paths_full, latency_capacity()};
}

bool Operations::enabled() {
    return Constants::profile_metrics || Constants::profile_top_n > 0 || Constants::profile_paths_full;
}

ProfilerCore& Operations::core() {
    static ProfilerCore collector(config(), static_cast<size_t>(OperationFunction::size));
    return collector;
}

void Operations::complete(OperationFunction func, uint64_t nanoseconds, ProfileOutcome result,
                          int error, const std::string& path, ProfileSource source) {
    if(!enabled()) return;
    core().complete(static_cast<size_t>(func), nanoseconds, result, error, path, source);
}

ProfilerSnapshot Operations::snapshot() { return core().snapshot(); }
ProfilerSnapshot Operations::snapshot_and_reset() { return core().snapshot_and_reset(); }

int Operations::get_operation_count(OperationFunction func) { return snapshot().operations[static_cast<size_t>(func)].count; }
uint64_t Operations::get_operation_time(OperationFunction func) { return snapshot().operations[static_cast<size_t>(func)].cumulative_ns; }
int Operations::get_operation_cache_hit(OperationFunction func) { return outcome(snapshot().operations[static_cast<size_t>(func)], ProfileOutcome::cache_hit); }
int Operations::get_operation_cache_miss(OperationFunction func) { return outcome(snapshot().operations[static_cast<size_t>(func)], ProfileOutcome::cache_miss); }
int Operations::get_operation_cache_neg(OperationFunction func) { return outcome(snapshot().operations[static_cast<size_t>(func)], ProfileOutcome::negative); }

// Starts are intentionally no-ops: only completion creates a request record.
void Operations::inc_operation(OperationFunction) {}
void Operations::reset_operation_counter() { snapshot_and_reset(); }
void Operations::reset_operation_timer() { snapshot_and_reset(); }
void Operations::reset_operation_cache_hit() { snapshot_and_reset(); }
void Operations::reset_operation_cache_miss() { snapshot_and_reset(); }
void Operations::reset_operation_cache_neg() { snapshot_and_reset(); }

std::ostream& Operations::log_operation(std::ostream& os) {
    const auto snap = snapshot();
    return log_snapshot("Aggregate Operation Counter", os, snap, [](const auto& op) { return op.count; });
}
std::ostream& Operations::log_operation_time(std::ostream& os) {
    const auto snap = snapshot();
    return log_snapshot("Aggregate Operation Cumulative Latency (nanoseconds)", os, snap, [](const auto& op) { return op.cumulative_ns; });
}
std::ostream& Operations::log_operation_cache_hit(std::ostream& os) {
    const auto snap = snapshot();
    return log_snapshot("Aggregate Operation Cache Hit", os, snap, [](const auto& op) { return outcome(op, ProfileOutcome::cache_hit); });
}
std::ostream& Operations::log_operation_cache_miss(std::ostream& os) {
    const auto snap = snapshot();
    return log_snapshot("Aggregate Operation Cache Miss", os, snap, [](const auto& op) { return outcome(op, ProfileOutcome::cache_miss); });
}
std::ostream& Operations::log_operation_neg(std::ostream& os) {
    const auto snap = snapshot();
    return log_snapshot("Aggregate Operation Cache Negative", os, snap, [](const auto& op) { return outcome(op, ProfileOutcome::negative); });
}
