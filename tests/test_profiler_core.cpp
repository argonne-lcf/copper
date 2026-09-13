#include "metric/profiler_core.h"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

static uint64_t outcome_sum(const PathSnapshot& event) {
    return std::accumulate(event.outcomes.begin(), event.outcomes.end(), uint64_t{0});
}

int main() {
    ProfilerCore p({2, false, 2});
    p.complete(0, 10, ProfileOutcome::cache_hit, 0, "a", ProfileSource::metadata);
    p.complete(0, 20, ProfileOutcome::cache_miss, ENOENT, "b", ProfileSource::metadata);
    p.complete(0, 30, ProfileOutcome::negative, EIO, "c", ProfileSource::metadata);
    p.complete(0, 40, ProfileOutcome::success, 0, "c", ProfileSource::metadata);
    p.complete(0, 50, ProfileOutcome::success, 0, "a", ProfileSource::metadata);
    p.record_ttl("a", MetadataTTLEvent::store);
    p.record_ttl("ttl-other", MetadataTTLEvent::serve);
    auto s=p.snapshot();
    CHECK(s.operations[0].count==5 && s.operations[0].cumulative_ns==150);
    CHECK(s.operations[0].latency.count==5);
    CHECK(s.operations[0].latency.sample_count==2);
    CHECK(s.operations[0].latency.sampling_method=="reservoir_algorithm_r");
    CHECK(s.operations[0].latency.min_ns==10 && s.operations[0].latency.max_ns==50);

    CHECK(s.paths_seen==7 && s.paths_retained==2 && s.paths_dropped==3 && s.other_events==3);
    CHECK(s.path_events.size()==2);
    uint64_t retained=0;
    for(const auto& event:s.path_events) { CHECK(event.total==outcome_sum(event)+std::accumulate(event.ttl_events.begin(),event.ttl_events.end(),uint64_t{0})); retained+=event.total; }
    CHECK(retained+s.other_events==s.paths_seen);
    CHECK(s.md_ttl_totals[static_cast<size_t>(MetadataTTLEvent::store)]==1);
    CHECK(s.md_ttl_totals[static_cast<size_t>(MetadataTTLEvent::serve)]==1);

    auto old=p.snapshot_and_reset();
    CHECK(old.operations[0].count==5);
    auto reset=p.snapshot();
    CHECK(reset.operations[0].count==0 && reset.paths_seen==0);
    CHECK(std::accumulate(reset.md_ttl_totals.begin(),reset.md_ttl_totals.end(),uint64_t{0})==0);

    ProfilerCore exact({1, true, 8});
    exact.complete(1, 5, ProfileOutcome::success, 0, "a", ProfileSource::data);
    exact.complete(1, 6, ProfileOutcome::success, 0, "b", ProfileSource::bypass);
    CHECK(exact.snapshot().path_events.size()==2);

    ProfilerCore quantiles({0, false, 8});
    for(uint64_t value : {10, 20, 30, 40, 50})
        quantiles.complete(0, value, ProfileOutcome::success, 0);
    const auto qs = quantiles.snapshot().operations[0].latency;
    CHECK(qs.p50_ns==30 && qs.p90_ns==50 && qs.p95_ns==50 && qs.p99_ns==50 && qs.p999_ns==50);

    ProfilerCore concurrent({8,false,32});
    std::vector<std::thread> ts;
    for(int t=0;t<8;++t) ts.emplace_back([&]{for(int i=0;i<1000;++i) concurrent.complete(2, i+1, ProfileOutcome::success, 0, std::to_string(i%16), ProfileSource::tree);});
    for(auto& t:ts)t.join();
    auto cs=concurrent.snapshot();
    CHECK(cs.operations[2].count==8000);
    uint64_t outcomes=0; for(auto v:cs.operations[2].outcomes) outcomes+=v;
    CHECK(outcomes==cs.operations[2].count);
    uint64_t path_total=cs.other_events; for(const auto& e:cs.path_events) path_total+=e.total;
    CHECK(path_total==cs.paths_seen);

    auto dir=std::filesystem::temp_directory_path()/"copper-profiler-test";
    std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
    auto result=write_snapshot_bundle_atomic(dir, "../host\n\t\"", 42, 7, "unit/snapshot", s, {2,false,2});
    CHECK(result.success && std::filesystem::exists(result.manifest_path));
    for(auto& entry:std::filesystem::directory_iterator(dir)) CHECK(entry.path().string().find(".tmp.")==std::string::npos);
    std::ifstream in(result.manifest_path); std::string text((std::istreambuf_iterator<char>(in)),{});
    CHECK(text.find("\"schema_version\": 2")!=std::string::npos);
    CHECK(text.find("../host\\n\\t\\\"")!=std::string::npos);
    CHECK(text.find("unit/snapshot")!=std::string::npos);
    CHECK(std::filesystem::path(result.manifest_path).parent_path()==dir);
    std::ifstream csv(dir/"___host___-42-7-unit_snapshot-profiling_operations.csv");
    std::string csv_text((std::istreambuf_iterator<char>(csv)),{});
    CHECK(csv_text.find("sample_count,sampling_method") != std::string::npos);
    std::ifstream paths(dir/"___host___-42-7-unit_snapshot-profiling_top_paths.csv");
    std::string paths_text((std::istreambuf_iterator<char>(paths)),{});
    CHECK(paths_text.find("ttl_stores,ttl_serves,ttl_expires,ttl_clears") != std::string::npos);
    std::ifstream aggregate(dir/"___host___-42-7-unit_snapshot-profiling_aggregate.csv");
    std::string aggregate_text((std::istreambuf_iterator<char>(aggregate)),{});
    CHECK(aggregate_text.find("md_enoent_ttl_stores,1") != std::string::npos);
    CHECK(aggregate_text.find("md_enoent_ttl_serves,1") != std::string::npos);
    std::filesystem::remove_all(dir);
    return 0;
}
