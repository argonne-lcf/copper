#ifndef COPPER_PROFILER_CORE_H
#define COPPER_PROFILER_CORE_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <vector>

enum class ProfileOutcome : size_t { success, cache_hit, cache_miss, negative, count };
enum class ProfileSource : size_t { none, metadata, data, tree, bypass, count };
enum class MetadataTTLEvent : size_t { store, serve, expire, clear, count };

// Inclusive integer bins: [0,0], [1,1], [2,3], ..., [2^63,UINT64_MAX].
struct LatencyHistogram {
    static constexpr size_t bucket_count = 65;
    std::array<uint64_t, bucket_count> counts{};
    static uint64_t lower_bound(size_t bucket);
    static uint64_t upper_bound(size_t bucket);
    void observe(uint64_t ns);
    void merge(const LatencyHistogram& other);
    uint64_t sample_count() const;
    uint64_t quantile_upper_bound(uint64_t numerator, uint64_t denominator) const;
};

struct LatencySnapshot {
    LatencyHistogram histogram;
    uint64_t count = 0;
    uint64_t sample_count = 0;
    std::string sampling_method = "reservoir_algorithm_r";
    uint64_t min_ns = 0, p50_ns = 0, p90_ns = 0, p95_ns = 0, p99_ns = 0, p999_ns = 0, max_ns = 0;
    uint64_t cumulative_ns = 0;
};
struct OperationSnapshot {
    uint64_t count = 0, cumulative_ns = 0;
    std::array<uint64_t, static_cast<size_t>(ProfileOutcome::count)> outcomes{};
    std::array<uint64_t, static_cast<size_t>(ProfileSource::count)> sources{};
    std::map<int, uint64_t> errors;
    LatencySnapshot latency;
};
struct PathSnapshot {
    ProfileSource source = ProfileSource::none;
    std::string path;
    uint64_t total = 0; // Exact observations since this key was last admitted.
    uint64_t estimated_total = 0, error_bound = 0;
    std::array<uint64_t, static_cast<size_t>(ProfileOutcome::count)> outcomes{};
    std::array<uint64_t, static_cast<size_t>(MetadataTTLEvent::count)> ttl_events{};
    std::map<int, uint64_t> errors;
};
struct ProfilerConfig { size_t top_n = 0; bool full_paths = false; size_t latency_capacity = 1024; bool heavy_hitters = false; };
struct ProfilerSnapshot {
    std::string semantics = "cumulative";
    std::string path_mode = "aggregate-only";
    uint64_t reset_epoch = 0;
    std::vector<OperationSnapshot> operations;
    std::vector<PathSnapshot> path_events;
    uint64_t paths_seen = 0, paths_retained = 0, paths_dropped = 0, other_events = 0;
    std::array<uint64_t, static_cast<size_t>(MetadataTTLEvent::count)> md_ttl_totals{};
};
struct ReportResult { bool success = false; std::string manifest_path, error; std::vector<std::string> files; };

class ProfilerCore {
public:
    explicit ProfilerCore(ProfilerConfig config, size_t operations = 64);
    void complete(size_t op, uint64_t ns, ProfileOutcome outcome, int error,
                  const std::string& path = "", ProfileSource source = ProfileSource::none);
    void record_ttl(const std::string& path, MetadataTTLEvent event);
    ProfilerSnapshot snapshot() const;
    ProfilerSnapshot snapshot_and_reset();
private:
    struct RawSnapshot { ProfilerSnapshot snapshot; std::vector<std::vector<uint64_t>> samples; };
    static ProfilerSnapshot finalize(RawSnapshot raw);
    void record_path_locked(const std::string&, ProfileSource, ProfileOutcome*, int, MetadataTTLEvent*);
    ProfilerConfig config_;
    mutable std::mutex mutex_;
    std::vector<OperationSnapshot> operations_;
    std::vector<std::vector<uint64_t>> samples_;
    std::map<std::pair<ProfileSource, std::string>, PathSnapshot> paths_;
    uint64_t reset_epoch_ = 0;
    uint64_t paths_seen_ = 0, paths_dropped_ = 0, other_events_ = 0;
    std::array<uint64_t, static_cast<size_t>(MetadataTTLEvent::count)> md_ttl_totals_{};
    std::mt19937_64 rng_{0x43505052ULL};
};

std::string profiler_sha256(const std::string& bytes);

ReportResult write_snapshot_bundle_atomic(const std::filesystem::path&, const std::string&, uint64_t,
                                          uint64_t, const std::string&, const ProfilerSnapshot&, const ProfilerConfig&);
#endif
