#include "profiling.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <vector>

namespace {
using PathOpEntry = std::pair<std::string, OpRes>;
using PathTTLEntry = std::pair<std::string, MDTTLEvent>;

struct PathClassStats {
    uint64_t entries = 0;
    uint64_t total_events = 0;
    std::string example_path;
};

uint64_t total_events(const OpRes& value) {
    return static_cast<uint64_t>(value.cache_hit) + static_cast<uint64_t>(value.cache_miss) + static_cast<uint64_t>(value.neg);
}

uint64_t total_events(const MDTTLEvent& value) {
    return value.store + value.serve + value.expire + value.clear;
}

std::vector<PathOpEntry> top_op_entries(std::unordered_map<std::string, OpRes>& table, int top_n) {
    std::vector<PathOpEntry> entries(table.begin(), table.end());
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return total_events(lhs.second) > total_events(rhs.second);
    });

    if(top_n > 0 && static_cast<int>(entries.size()) > top_n) {
        entries.resize(top_n);
    }
    return entries;
}

std::vector<PathTTLEntry> top_ttl_entries(std::unordered_map<std::string, MDTTLEvent>& table, int top_n) {
    std::vector<PathTTLEntry> entries(table.begin(), table.end());
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return total_events(lhs.second) > total_events(rhs.second);
    });

    if(top_n > 0 && static_cast<int>(entries.size()) > top_n) {
        entries.resize(top_n);
    }
    return entries;
}

std::string csv_escape(const std::string& value) {
    std::string escaped = "\"";
    for(char ch : value) {
        if(ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
}

std::string sanitize_tag(std::string tag) {
    if(tag.empty()) {
        return tag;
    }

    for(char& ch : tag) {
        const bool keep = std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_';
        if(!keep) {
            ch = '_';
        }
    }
    return tag;
}

bool contains_token(const std::string& value, const std::string& token) {
    return value.find(token) != std::string::npos;
}

bool ends_with(const std::string& value, const std::string& suffix) {
    if(suffix.size() > value.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string classify_path(const std::string& category, const std::string& path) {
    if(category == "metadata_ttl") {
        if(contains_token(path, "python") && ends_with(path, ".zip")) {
            return "python_zip_probe";
        }
        if(ends_with(path, "/pyvenv.cfg")) {
            return "virtual_env_probe";
        }
        if(contains_token(path, "glibc-hwcaps")) {
            return "glibc_hwcaps_probe";
        }
        if(contains_token(path, ".so")) {
            return "missing_shared_library_probe";
        }
        return "negative_probe_path";
    }

    if(contains_token(path, "/site-packages/torch/lib/")) {
        return "torch_native_library";
    }
    if(contains_token(path, "/site-packages/torch")) {
        return "torch_python_package";
    }
    if(contains_token(path, "/site-packages")) {
        return "python_site_packages";
    }
    if(contains_token(path, "/lib/python")) {
        return "python_stdlib";
    }
    if(contains_token(path, "/bin/")) {
        return "environment_bin";
    }
    if(contains_token(path, ".so")) {
        return "shared_library";
    }
    return "environment_prefix";
}

void update_stats(std::unordered_map<std::string, PathClassStats>& stats,
                  const std::string& path_class,
                  const std::string& path,
                  uint64_t event_count) {
    auto& entry = stats[path_class];
    entry.entries++;
    entry.total_events += event_count;
    if(entry.example_path.empty()) {
        entry.example_path = path;
    }
}
} // namespace

bool Profiling::enabled() {
    return Constants::profile_metrics || Constants::profile_top_n > 0 || Constants::profile_paths_full;
}

bool Profiling::top_paths_enabled() {
    return Constants::profile_top_n > 0;
}

bool Profiling::full_paths_enabled() {
    return Constants::profile_paths_full;
}

void Profiling::record_md_ttl_store(const std::string& path) {
    if(!enabled()) {
        return;
    }

    std::lock_guard<std::mutex> guard(profiling_mtx);
    md_ttl_store_count++;
    if(CacheEvent::should_record_paths()) {
        md_ttl_event_table[path].store++;
    }
}

void Profiling::record_md_ttl_serve(const std::string& path) {
    if(!enabled()) {
        return;
    }

    std::lock_guard<std::mutex> guard(profiling_mtx);
    md_ttl_serve_count++;
    if(CacheEvent::should_record_paths()) {
        md_ttl_event_table[path].serve++;
    }
}

void Profiling::record_md_ttl_expire(const std::string& path) {
    if(!enabled()) {
        return;
    }

    std::lock_guard<std::mutex> guard(profiling_mtx);
    md_ttl_expire_count++;
    if(CacheEvent::should_record_paths()) {
        md_ttl_event_table[path].expire++;
    }
}

void Profiling::record_md_ttl_clear(const std::string& path) {
    if(!enabled()) {
        return;
    }

    std::lock_guard<std::mutex> guard(profiling_mtx);
    md_ttl_clear_count++;
    if(CacheEvent::should_record_paths()) {
        md_ttl_event_table[path].clear++;
    }
}

uint64_t Profiling::get_md_ttl_store_count() {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    return md_ttl_store_count;
}

uint64_t Profiling::get_md_ttl_serve_count() {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    return md_ttl_serve_count;
}

uint64_t Profiling::get_md_ttl_expire_count() {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    return md_ttl_expire_count;
}

uint64_t Profiling::get_md_ttl_clear_count() {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    return md_ttl_clear_count;
}

std::ostream& Profiling::log_md_ttl_event(std::ostream& os) {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    os << "Metadata ENOENT TTL Events {" << std::endl;
    for(const auto& entry : md_ttl_event_table) {
        os << entry.first << " {" << std::endl;
        os << "stores: " << entry.second.store << "," << std::endl;
        os << "serves: " << entry.second.serve << "," << std::endl;
        os << "expires: " << entry.second.expire << "," << std::endl;
        os << "clears: " << entry.second.clear << std::endl;
        os << "}" << std::endl;
    }
    os << "}";
    return os;
}

std::string Profiling::mode_string() {
    if(full_paths_enabled()) {
        return "full-path";
    }
    if(top_paths_enabled()) {
        return "top-path";
    }
    if(enabled()) {
        return "aggregate-only";
    }
    return "disabled";
}

std::string Profiling::tagged_filename(const std::string& base_filename, const std::string& snapshot_tag) {
    const std::string clean_tag = sanitize_tag(snapshot_tag);
    if(clean_tag.empty()) {
        return Constants::get_output_filename(base_filename);
    }

    return Constants::my_hostname + "-" + Constants::pid + "-" + clean_tag + "-" + base_filename;
}

void Profiling::write_operations_csv(const std::string& output_path) {
    std::ofstream os(output_path, std::ios::out | std::ios::trunc);
    if(!os.is_open()) {
        LOG(ERROR) << "failed to open profiling operations csv: " << output_path << std::endl;
        return;
    }

    os << "operation,total_calls,cache_hits,cache_misses,negative_results,avg_latency_us,total_latency_s\n";
    for(int i = 0; i < static_cast<int>(OperationFunction::size); ++i) {
        const auto func = static_cast<OperationFunction>(i);
        const auto total = Operations::get_operation_count(func);
        if(total == 0) {
            continue;
        }

        const auto hits = Operations::get_operation_cache_hit(func);
        const auto misses = Operations::get_operation_cache_miss(func);
        const auto neg = Operations::get_operation_cache_neg(func);
        const auto total_us = Operations::get_operation_time(func);
        const auto avg_us = (total > 0) ? static_cast<double>(total_us) / total : 0.0;
        const auto total_s = static_cast<double>(total_us) / 1000000.0;

        os << OperationFunctionNames[i] << ","
           << total << ","
           << hits << ","
           << misses << ","
           << neg << ","
           << std::fixed << std::setprecision(2) << avg_us << ","
           << std::fixed << std::setprecision(6) << total_s << "\n";
    }
}

void Profiling::write_top_paths_csv(const std::string& output_path) {
    std::ofstream os(output_path, std::ios::out | std::ios::trunc);
    if(!os.is_open()) {
        LOG(ERROR) << "failed to open profiling top-path csv: " << output_path << std::endl;
        return;
    }

    os << "category,path,total_events,cache_hits,cache_misses,negative_results,ttl_stores,ttl_serves,ttl_expires,ttl_clears\n";

    const int top_n = Constants::profile_top_n;
    {
        std::lock_guard<std::mutex> guard(CacheEvent::cache_event_mtx);
        for(const auto& entry : top_op_entries(CacheEvent::md_cache_event_table, top_n)) {
            os << "metadata,"
               << csv_escape(entry.first) << ","
               << total_events(entry.second) << ","
               << entry.second.cache_hit << ","
               << entry.second.cache_miss << ","
               << entry.second.neg << ",0,0,0,0\n";
        }
        for(const auto& entry : top_op_entries(CacheEvent::data_cache_event_table, top_n)) {
            os << "data,"
               << csv_escape(entry.first) << ","
               << total_events(entry.second) << ","
               << entry.second.cache_hit << ","
               << entry.second.cache_miss << ","
               << entry.second.neg << ",0,0,0,0\n";
        }
        for(const auto& entry : top_op_entries(CacheEvent::tree_cache_event_table, top_n)) {
            os << "tree,"
               << csv_escape(entry.first) << ","
               << total_events(entry.second) << ","
               << entry.second.cache_hit << ","
               << entry.second.cache_miss << ","
               << entry.second.neg << ",0,0,0,0\n";
        }
    }

    {
        std::lock_guard<std::mutex> guard(profiling_mtx);
        for(const auto& entry : top_ttl_entries(md_ttl_event_table, top_n)) {
            os << "metadata_ttl,"
               << csv_escape(entry.first) << ","
               << total_events(entry.second) << ",0,0,0,"
               << entry.second.store << ","
               << entry.second.serve << ","
               << entry.second.expire << ","
               << entry.second.clear << "\n";
        }
    }
}

void Profiling::write_aggregate_csv(const std::string& output_path) {
    std::ofstream os(output_path, std::ios::out | std::ios::trunc);
    if(!os.is_open()) {
        LOG(ERROR) << "failed to open profiling aggregate csv: " << output_path << std::endl;
        return;
    }

    os << "metric,value\n";
    uint64_t total_calls = 0;
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    uint64_t total_neg = 0;
    long total_us = 0;

    for(int i = 0; i < static_cast<int>(OperationFunction::size); ++i) {
        const auto func = static_cast<OperationFunction>(i);
        const auto count = Operations::get_operation_count(func);
        if(count == 0) {
            continue;
        }

        total_calls += count;
        total_hits += Operations::get_operation_cache_hit(func);
        total_misses += Operations::get_operation_cache_miss(func);
        total_neg += Operations::get_operation_cache_neg(func);
        total_us += Operations::get_operation_time(func);
    }

    os << "total_counted_fuse_operations," << total_calls << "\n";
    os << "total_cache_hits," << total_hits << "\n";
    os << "total_cache_misses," << total_misses << "\n";
    os << "total_negative_results," << total_neg << "\n";
    os << "estimated_cache_served_operations_saved_by_copper," << total_hits << "\n";
    os << "metadata_enoent_ttl_stores," << get_md_ttl_store_count() << "\n";
    os << "metadata_enoent_ttl_serves," << get_md_ttl_serve_count() << "\n";
    os << "metadata_enoent_ttl_expires," << get_md_ttl_expire_count() << "\n";
    os << "metadata_enoent_ttl_clears_on_success_or_non_enoent," << get_md_ttl_clear_count() << "\n";
    os << "estimated_repeated_metadata_lookups_avoided_by_ttl," << get_md_ttl_serve_count() << "\n";
    os << "total_measured_latency_seconds," << std::fixed << std::setprecision(6) << static_cast<double>(total_us) / 1000000.0 << "\n";
}

void Profiling::write_summary_md(const std::string& output_path, const std::string& snapshot_tag) {
    std::ofstream os(output_path, std::ios::out | std::ios::trunc);
    if(!os.is_open()) {
        LOG(ERROR) << "failed to open profiling summary markdown: " << output_path << std::endl;
        return;
    }

    uint64_t total_calls = 0;
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    uint64_t total_neg = 0;
    long total_us = 0;

    os << "# Copper Profiling Summary\n\n";
    os << "- Profiling mode: `" << mode_string() << "`\n";
    os << "- Metadata ENOENT TTL (ms): `" << Constants::md_enoent_ttl_ms << "`\n";
    os << "- Top-path limit: `" << Constants::profile_top_n << "`\n";
    os << "- Full path outputs enabled: `" << (Constants::profile_paths_full ? "yes" : "no") << "`\n\n";
    if(!snapshot_tag.empty()) {
        os << "- Snapshot tag: `" << snapshot_tag << "`\n\n";
    }

    os << "## Aggregate Operation Metrics\n\n";
    os << "| Operation | Total Calls | Cache Hits | Cache Misses | Negative Results | Avg Latency (us) | Total Latency (s) |\n";
    os << "|---|---:|---:|---:|---:|---:|---:|\n";
    for(int i = 0; i < static_cast<int>(OperationFunction::size); ++i) {
        const auto func = static_cast<OperationFunction>(i);
        const auto count = Operations::get_operation_count(func);
        if(count == 0) {
            continue;
        }

        const auto hits = Operations::get_operation_cache_hit(func);
        const auto misses = Operations::get_operation_cache_miss(func);
        const auto neg = Operations::get_operation_cache_neg(func);
        const auto op_total_us = Operations::get_operation_time(func);
        const auto avg_us = (count > 0) ? static_cast<double>(op_total_us) / count : 0.0;
        const auto op_total_s = static_cast<double>(op_total_us) / 1000000.0;

        total_calls += count;
        total_hits += hits;
        total_misses += misses;
        total_neg += neg;
        total_us += op_total_us;

        os << "| `" << OperationFunctionNames[i] << "` | "
           << count << " | "
           << hits << " | "
           << misses << " | "
           << neg << " | "
           << std::fixed << std::setprecision(2) << avg_us << " | "
           << std::fixed << std::setprecision(6) << op_total_s << " |\n";
    }

    os << "\n## Derived High-Level Metrics\n\n";
    os << "| Metric | Value |\n";
    os << "|---|---:|\n";
    os << "| Total counted FUSE operations | " << total_calls << " |\n";
    os << "| Total cache hits | " << total_hits << " |\n";
    os << "| Total cache misses | " << total_misses << " |\n";
    os << "| Total negative results | " << total_neg << " |\n";
    os << "| Estimated cache-served operations saved by Copper | " << total_hits << " |\n";
    os << "| Metadata ENOENT TTL stores | " << get_md_ttl_store_count() << " |\n";
    os << "| Metadata ENOENT TTL serves | " << get_md_ttl_serve_count() << " |\n";
    os << "| Metadata ENOENT TTL expires | " << get_md_ttl_expire_count() << " |\n";
    os << "| Metadata ENOENT TTL clears on success/non-ENOENT | " << get_md_ttl_clear_count() << " |\n";
    os << "| Estimated repeated metadata lookups avoided by TTL | " << get_md_ttl_serve_count() << " |\n";
    os << "| Total measured latency across counted operations (s) | "
       << std::fixed << std::setprecision(6) << static_cast<double>(total_us) / 1000000.0 << " |\n";

    if(top_paths_enabled()) {
        os << "\n## Top Paths\n\n";
        os << "The detailed top-path CSV is written to `"
           << tagged_filename(Constants::log_profile_top_paths_output_filename, snapshot_tag)
           << "`.\n";
    }

    if(full_paths_enabled()) {
        os << "\n## Full Path Outputs\n\n";
        os << "Full path-level cache-event and TTL-event outputs were also written because `-profile_paths_full` was enabled.\n";
    }

    write_path_guidance_md(os);
}

void Profiling::write_path_guidance_md(std::ostream& os) {
    if(!top_paths_enabled()) {
        return;
    }

    std::unordered_map<std::string, PathClassStats> class_stats;
    uint64_t metadata_ttl_serves = 0;
    uint64_t missing_shared_lib_probe_events = 0;
    uint64_t python_probe_events = 0;

    {
        std::lock_guard<std::mutex> guard(CacheEvent::cache_event_mtx);
        for(const auto& entry : top_op_entries(CacheEvent::md_cache_event_table, Constants::profile_top_n)) {
            update_stats(class_stats, classify_path("metadata", entry.first), entry.first, total_events(entry.second));
        }
        for(const auto& entry : top_op_entries(CacheEvent::data_cache_event_table, Constants::profile_top_n)) {
            update_stats(class_stats, classify_path("data", entry.first), entry.first, total_events(entry.second));
        }
        for(const auto& entry : top_op_entries(CacheEvent::tree_cache_event_table, Constants::profile_top_n)) {
            update_stats(class_stats, classify_path("tree", entry.first), entry.first, total_events(entry.second));
        }
    }

    {
        std::lock_guard<std::mutex> guard(profiling_mtx);
        for(const auto& entry : top_ttl_entries(md_ttl_event_table, Constants::profile_top_n)) {
            const std::string path_class = classify_path("metadata_ttl", entry.first);
            const uint64_t event_count = total_events(entry.second);
            update_stats(class_stats, path_class, entry.first, event_count);
            metadata_ttl_serves += entry.second.serve;
            if(path_class == "missing_shared_library_probe") {
                missing_shared_lib_probe_events += event_count;
            }
            if(path_class == "python_zip_probe" || path_class == "virtual_env_probe" || path_class == "glibc_hwcaps_probe") {
                python_probe_events += event_count;
            }
        }
    }

    if(class_stats.empty()) {
        return;
    }

    std::vector<std::pair<std::string, PathClassStats>> sorted_stats(class_stats.begin(), class_stats.end());
    std::sort(sorted_stats.begin(), sorted_stats.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.second.total_events > rhs.second.total_events;
    });

    os << "\n## Path Class Heuristics\n\n";
    os << "This section groups the hottest observed paths into coarse classes so users can reason about what to prune and what to keep.\n\n";
    os << "| Path Class | Entries | Total Events | Example |\n";
    os << "|---|---:|---:|---|\n";
    for(const auto& entry : sorted_stats) {
        os << "| `" << entry.first << "` | "
           << entry.second.entries << " | "
           << entry.second.total_events << " | `"
           << entry.second.example_path << "` |\n";
    }

    os << "\n## Pruning Guidance\n\n";
    os << "These are heuristic suggestions derived from the observed top paths and TTL paths. They are intended to guide a staged cleanup, not to justify deleting unseen files from the original environment.\n\n";
    os << "- Keep the active environment core paths intact first: environment root, `bin`, `lib`, `lib/python*`, and `site-packages`.\n";

    if(class_stats.count("torch_native_library") > 0) {
        os << "- `torch_native_library` paths are hot. This workload is relying on native Torch shared libraries, so pruning should focus around the environment rather than removing Torch library directories.\n";
    }
    if(class_stats.count("python_site_packages") > 0 || class_stats.count("python_stdlib") > 0) {
        os << "- Python package and stdlib paths are active. If pruning is desired, prefer removing duplicate `PYTHONPATH` entries and unrelated packages before touching the core interpreter tree.\n";
    }
    if(metadata_ttl_serves > 0) {
        os << "- Repeated ENOENT probe paths are present. High TTL serves suggest it is worth auditing `LD_LIBRARY_PATH`, `PATH`, and `PYTHONPATH` for duplicate or stale entries, while keeping the ENOENT TTL enabled.\n";
    }
    if(missing_shared_lib_probe_events > 0) {
        os << "- Missing shared-library probes were observed. The lowest-risk cleanup is usually to shorten `LD_LIBRARY_PATH`, remove stale runtime/toolchain directories, and avoid duplicate library search roots.\n";
    }
    if(python_probe_events > 0) {
        os << "- Python startup probe paths such as `python*.zip`, `pyvenv.cfg`, or `glibc-hwcaps` were observed. These are usually normal probes, so they are better treated as optimization hints than as application bugs.\n";
    }
    os << "- Recommended pruning order: remove duplicate entries first, then nonexistent entries, then stale environment/toolchain paths, and only then experiment with a reduced copy or allowlist-based tree.\n";
    os << "- If running a second minimization experiment, use the observed paths as an initial allowlist and test in a cloned or filtered environment instead of pruning the original tree in place.\n";
}

void Profiling::write_full_path_outputs(const std::string& path_string, const std::string& snapshot_tag) {
    if(!full_paths_enabled()) {
        return;
    }

    const std::string clean_tag = sanitize_tag(snapshot_tag);
    const auto output_dir = std::filesystem::path(Constants::tables_dir()) /
                            (clean_tag.empty() ? Constants::profiling_final_subdir : clean_tag);
    std::filesystem::create_directories(output_dir);

    {
        std::ofstream os((output_dir / tagged_filename(Constants::log_data_cache_event_output_filename, snapshot_tag)).string(), std::ios::out | std::ios::trunc);
        if(os.is_open()) {
            os << CacheEvent::log_data_cache_event;
        }
    }
    {
        std::ofstream os((output_dir / tagged_filename(Constants::log_tree_cache_event_output_filename, snapshot_tag)).string(), std::ios::out | std::ios::trunc);
        if(os.is_open()) {
            os << CacheEvent::log_tree_cache_event;
        }
    }
    {
        std::ofstream os((output_dir / tagged_filename(Constants::log_md_cache_event_output_filename, snapshot_tag)).string(), std::ios::out | std::ios::trunc);
        if(os.is_open()) {
            os << CacheEvent::log_md_cache_event;
        }
    }
    {
        std::ofstream os((output_dir / tagged_filename(Constants::log_md_ttl_event_output_filename, snapshot_tag)).string(), std::ios::out | std::ios::trunc);
        if(os.is_open()) {
            os << log_md_ttl_event;
        }
    }
}

void Profiling::write_reports(const std::string& path_string, const std::string& snapshot_tag) {
    if(!enabled()) {
        return;
    }

    const std::string clean_tag = sanitize_tag(snapshot_tag);
    const auto output_dir = std::filesystem::path(Constants::profiling_snapshot_dir(clean_tag));
    std::filesystem::create_directories(output_dir);
    write_summary_md((output_dir / tagged_filename(Constants::log_profile_summary_output_filename, snapshot_tag)).string(), snapshot_tag);
    write_operations_csv((output_dir / tagged_filename(Constants::log_profile_operations_output_filename, snapshot_tag)).string());
    write_aggregate_csv((output_dir / tagged_filename(Constants::log_profile_aggregate_output_filename, snapshot_tag)).string());

    if(top_paths_enabled()) {
        write_top_paths_csv((output_dir / tagged_filename(Constants::log_profile_top_paths_output_filename, snapshot_tag)).string());
    }

    write_full_path_outputs(path_string, snapshot_tag);
}

void Profiling::reset() {
    std::lock_guard<std::mutex> guard(profiling_mtx);
    md_ttl_event_table.clear();
    md_ttl_store_count = 0;
    md_ttl_serve_count = 0;
    md_ttl_expire_count = 0;
    md_ttl_clear_count = 0;
}
