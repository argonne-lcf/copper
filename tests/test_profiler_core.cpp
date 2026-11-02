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
    CHECK(profiler_sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(profiler_sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(profiler_sha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // Invalid inputs and integer overflow fail without silently wrapping accounting.
    ProfilerCore overflow_core({0,false,0});
    overflow_core.complete(0,UINT64_MAX,ProfileOutcome::success,0);
    bool overflow_failed=false;
    try { overflow_core.complete(0,1,ProfileOutcome::success,0); } catch(const std::overflow_error&) { overflow_failed=true; }
    CHECK(overflow_failed && overflow_core.snapshot().operations[0].count==1);
    bool invalid_failed=false;
    try { overflow_core.complete(0,0,ProfileOutcome::count,0); } catch(const std::invalid_argument&) { invalid_failed=true; }
    CHECK(invalid_failed);
    bool ttl_failed=false;
    try { overflow_core.record_ttl("a",MetadataTTLEvent::count); } catch(const std::invalid_argument&) { ttl_failed=true; }
    CHECK(ttl_failed);
    bool config_failed=false;
    try { ProfilerCore invalid_config({0,false,0,true}); } catch(const std::invalid_argument&) { config_failed=true; }
    CHECK(config_failed);
    config_failed=false;
    try { ProfilerCore invalid_config({1000001,false,0}); } catch(const std::invalid_argument&) { config_failed=true; }
    CHECK(config_failed);
    // Exact mergeable histogram is independent of reservoir capacity.
    ProfilerCore histogram_core({0, false, 0});
    for(uint64_t ns : {uint64_t{0}, uint64_t{1}, uint64_t{2}, uint64_t{3}, uint64_t{4}, uint64_t{1} << 63})
        histogram_core.complete(0, ns, ProfileOutcome::success, 0);
    auto histogram = histogram_core.snapshot().operations[0].latency.histogram;
    CHECK(histogram.counts[0]==1 && histogram.counts[1]==1 && histogram.counts[2]==2);
    CHECK(histogram.counts[3]==1 && histogram.counts[64]==1);
    CHECK(histogram.sample_count()==6);
    CHECK(LatencyHistogram::lower_bound(64)==(uint64_t{1}<<63));
    CHECK(LatencyHistogram::upper_bound(64)==UINT64_MAX);
    CHECK(histogram.quantile_upper_bound(50,100)==3);
    auto merged = histogram;
    merged.merge(histogram);
    CHECK(merged.sample_count()==12 && merged.quantile_upper_bound(50,100)==3);
    CHECK(histogram_core.snapshot_and_reset().operations[0].latency.histogram.sample_count()==6);
    CHECK(histogram_core.snapshot().operations[0].latency.histogram.sample_count()==0);

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
    CHECK(old.semantics=="interval" && old.reset_epoch==0);
    CHECK(p.snapshot().semantics=="cumulative" && p.snapshot().reset_epoch==1);
    auto reset=p.snapshot();
    CHECK(reset.operations[0].count==0 && reset.paths_seen==0);
    CHECK(std::accumulate(reset.md_ttl_totals.begin(),reset.md_ttl_totals.end(),uint64_t{0})==0);

    setenv("COPPER_PROFILE_PATH_MODE","heavy-hitter",1);
    ProfilerCore environment_heavy({2,false,0});
    CHECK(environment_heavy.snapshot().path_mode=="bounded-heavy-hitter");
    unsetenv("COPPER_PROFILE_PATH_MODE");
    ProfilerCore heavy({2, false, 0, true});
    heavy.complete(0,1,ProfileOutcome::success,0,"early-a");
    heavy.complete(0,1,ProfileOutcome::success,0,"early-b");
    for(int i=0;i<100;++i) heavy.complete(0,1,ProfileOutcome::cache_hit,0,"late-hot");
    auto hs=heavy.snapshot();
    CHECK(hs.path_mode=="bounded-heavy-hitter" && hs.path_events.size()==2);
    CHECK(hs.path_events[0].path=="late-hot");
    CHECK(hs.path_events[0].estimated_total==101 && hs.path_events[0].error_bound==1);
    CHECK(hs.path_events[0].total==100 && hs.path_events[0].outcomes[1]==100);
    uint64_t htotal=hs.other_events;for(const auto& e:hs.path_events)htotal+=e.total;
    CHECK(htotal==hs.paths_seen && hs.paths_seen==102 && hs.other_events==1);
    CHECK(heavy.snapshot_and_reset().path_events.size()==2);
    CHECK(heavy.snapshot().path_events.empty() && heavy.snapshot().other_events==0);

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

    unsetenv("COPPER_RUN_ID"); unsetenv("COPPER_DAEMON_GENERATION");
    auto dir=std::filesystem::temp_directory_path()/"copper-profiler-test";
    std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
    auto result=write_snapshot_bundle_atomic(dir, "../host\n\t\"", 42, 7, "unit/snapshot", s, {2,false,2});
    CHECK(result.success && std::filesystem::exists(result.manifest_path));
    for(auto& entry:std::filesystem::directory_iterator(dir)) CHECK(entry.path().string().find(".tmp.")==std::string::npos);
    std::ifstream in(result.manifest_path); std::string text((std::istreambuf_iterator<char>(in)),{});
    CHECK(text.find("\"schema_version\": 3")!=std::string::npos);
    CHECK(text.find("\"semantics\": \"cumulative\"")!=std::string::npos);
    CHECK(text.find("\"run_id\": null")!=std::string::npos);
    CHECK(text.find("\"daemon_generation\": null")!=std::string::npos);
    CHECK(text.find("\"file_integrity\"")!=std::string::npos);
    CHECK(result.files.size()==5);
    for(const auto& file:result.files) if(file!=result.manifest_path) {
        std::ifstream data(file,std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(data)),{});
        CHECK(text.find(profiler_sha256(bytes))!=std::string::npos);
    }
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
    setenv("COPPER_RUN_ID","run-\"test",1); setenv("COPPER_DAEMON_GENERATION","generation-2",1);
    auto interval_result=write_snapshot_bundle_atomic(dir,"host",42,8,"interval",old,{2,false,2});
    CHECK(interval_result.success);
    std::ifstream interval_in(interval_result.manifest_path);
    std::string interval_text((std::istreambuf_iterator<char>(interval_in)),{});
    CHECK(interval_text.find("\"semantics\": \"interval\"")!=std::string::npos);
    CHECK(interval_text.find("run-\\\"test")!=std::string::npos);
    CHECK(interval_text.find("generation-2")!=std::string::npos);
    ProfilerCore report_overflow({0,false,0},2);
    report_overflow.complete(0,UINT64_MAX,ProfileOutcome::success,0);
    report_overflow.complete(1,1,ProfileOutcome::success,0);
    auto bad_report=write_snapshot_bundle_atomic(dir,"overflow",42,9,"final",report_overflow.snapshot(),{0,false,0});
    CHECK(!bad_report.success && !bad_report.error.empty());
    auto duplicate=write_snapshot_bundle_atomic(dir,"host",42,8,"interval",old,{2,false,2});
    CHECK(!duplicate.success && !duplicate.error.empty());
    std::filesystem::remove_all(dir);
    return 0;
}
