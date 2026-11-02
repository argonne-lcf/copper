#include "metric/profiler_core.h"
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sys/resource.h>
#include <thread>
#include <vector>

// Microbenchmark only: fixed paths and ns eliminate filesystem and clock costs.
// Exactly one case per process: ru_maxrss is a process-lifetime high-water mark.
int main(int argc, char** argv) {
    uint64_t iterations = 100000;
    size_t threads = 1;
    std::string mode = "aggregate";
    uint64_t snapshot_count = 10;
    try {
        const auto positive_count = [](const std::string& text, uint64_t maximum) {
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("counts must be positive decimal integers");
            const auto value = std::stoull(text);
            if (value == 0 || value > maximum)
                throw std::invalid_argument("count outside supported range");
            return value;
        };
        std::vector<std::string> seen;
        for (int i = 1; i < argc; i += 2) {
            const std::string option = argv[i];
            if (i + 1 == argc) throw std::invalid_argument("missing option value");
            for (const auto& previous : seen)
                if (previous == option) throw std::invalid_argument("duplicate option");
            seen.push_back(option);
            if (option == "--mode") mode = argv[i + 1];
            else if (option == "--threads") threads = positive_count(argv[i + 1], 8);
            else if (option == "--iterations") iterations = positive_count(argv[i + 1], 1000000000);
            else if (option == "--snapshots") snapshot_count = positive_count(argv[i + 1], 1000000);
            else throw std::invalid_argument("unknown option: " + option);
        }
        if (threads != 1 && threads != 2 && threads != 4 && threads != 8)
            throw std::invalid_argument("threads must be 1, 2, 4 or 8");
        if (mode != "disabled" && mode != "aggregate" && mode != "admitted" &&
            mode != "bounded" && mode != "full")
            throw std::invalid_argument("unknown mode: " + mode);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what()
                  << "\nusage: benchmark-profiler [--mode disabled|aggregate|admitted|bounded|full]"
                     " [--threads 1|2|4|8] [--iterations 1..1000000000]"
                     " [--snapshots 1..1000000]\n";
        return 2;
    }
    unsetenv("COPPER_PROFILE_PATH_MODE");
    const bool enabled = mode != "disabled";
    ProfilerConfig config{mode == "bounded" || mode == "admitted" ? 64u : 0u,
                          mode == "full", 1024, mode == "bounded"};
    ProfilerCore core(config, 4);
    std::vector<std::string> paths;
    for (int i = 0; i < 1024; ++i)
        paths.push_back("/benchmark/path/" + std::to_string(i));
    std::barrier gate(static_cast<std::ptrdiff_t>(threads + 1));
    std::vector<std::thread> workers;
    std::vector<uint64_t> checksums(threads);
    for (size_t t = 0; t < threads; ++t) workers.emplace_back([&, t] {
        gate.arrive_and_wait();
        uint64_t sum = 0;
        for (uint64_t i = 0; i < iterations; ++i) {
            sum += i;
            // 80% hot paths, 20% rotating cold keys.
            const auto& path = paths[(i % 5) ? i % 16 : 16 + (i % 1008)];
            if (enabled)
                core.complete(t % 4, i % 1024 + 1, ProfileOutcome::success,
                              0, path, ProfileSource::data);
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }
        checksums[t] = sum;
    });
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    gate.arrive_and_wait();
    for (auto& worker : workers) worker.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - start).count();
    uint64_t snapshot_elapsed = 0;
    ProfilerSnapshot snapshot;
    for (uint64_t i = 0; i < snapshot_count; ++i) {
        const auto snapshot_start = Clock::now();
        auto current = core.snapshot();
        snapshot_elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - snapshot_start).count();
        snapshot = std::move(current);
    }
    uint64_t calls = 0;
    for (const auto& operation : snapshot.operations) calls += operation.count;
    if (calls != (enabled ? threads * iterations : 0)) return 3;
    for (auto checksum : checksums)
        if (checksum != iterations * (iterations - 1) / 2) return 4;
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 5;
    // Linux reports KiB; macOS reports bytes. This includes setup, threads,
    // collector and snapshot allocations, NOT collector-only memory.
#ifdef __APPLE__
    const auto peak_rss_kib = usage.ru_maxrss / 1024;
#else
    const auto peak_rss_kib = usage.ru_maxrss;
#endif
    std::cout << "mode,threads,iterations_per_thread,total_calls,elapsed_ns,ns_per_call,"
                 "paths_retained,snapshot_count,snapshot_elapsed_ns,ns_per_snapshot,process_peak_rss_kib\n"
              << std::fixed << std::setprecision(6)
              << mode << ',' << threads << ',' << iterations << ',' << calls << ','
              << elapsed << ',' << double(elapsed) / double(threads * iterations) << ','
              << snapshot.paths_retained << ',' << snapshot_count << ',' << snapshot_elapsed << ','
              << double(snapshot_elapsed) / double(snapshot_count) << ',' << peak_rss_kib << '\n';
}
