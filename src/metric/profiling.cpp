#include "profiling.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <sstream>
#include <vector>
#include <unistd.h>

#include "../aixlog.h"
namespace {
std::atomic<uint64_t> report_sequence{0};
} // namespace

bool Profiling::enabled() { return Operations::enabled(); }
bool Profiling::top_paths_enabled() { return Constants::profile_top_n > 0; }
bool Profiling::full_paths_enabled() { return Constants::profile_paths_full; }
std::string Profiling::mode_string() {
    if(full_paths_enabled()) return "full-path";
    if(top_paths_enabled()) return "top-path";
    return enabled() ? "aggregate-only" : "disabled";
}

void Profiling::record_md_ttl_store(const std::string& p) { if(enabled()) Operations::core().record_ttl(p, MetadataTTLEvent::store); }
void Profiling::record_md_ttl_serve(const std::string& p) { if(enabled()) Operations::core().record_ttl(p, MetadataTTLEvent::serve); }
void Profiling::record_md_ttl_expire(const std::string& p) { if(enabled()) Operations::core().record_ttl(p, MetadataTTLEvent::expire); }
void Profiling::record_md_ttl_clear(const std::string& p) { if(enabled()) Operations::core().record_ttl(p, MetadataTTLEvent::clear); }
uint64_t Profiling::get_md_ttl_store_count() { return Operations::snapshot().md_ttl_totals[0]; }
uint64_t Profiling::get_md_ttl_serve_count() { return Operations::snapshot().md_ttl_totals[1]; }
uint64_t Profiling::get_md_ttl_expire_count() { return Operations::snapshot().md_ttl_totals[2]; }
uint64_t Profiling::get_md_ttl_clear_count() { return Operations::snapshot().md_ttl_totals[3]; }

std::ostream& Profiling::log_md_ttl_event(std::ostream& os) {
    const auto snapshot = Operations::snapshot();
    os << "Metadata ENOENT TTL Events {\n";
    for(const auto& event : snapshot.path_events) if(event.source == ProfileSource::metadata) {
        os << event.path << " {\nstores: " << event.ttl_events[0] << ",\nserves: " << event.ttl_events[1]
           << ",\nexpires: " << event.ttl_events[2] << ",\nclears: " << event.ttl_events[3] << "\n}\n";
    }
    return os << '}';
}

ReportResult Profiling::write_reports(const std::string&, const std::string& snapshot_tag) {
    ReportResult result;
    if(!enabled()) { result.error = "profiling disabled"; return result; }
    const auto snapshot = Operations::snapshot();
    const uint64_t sequence = report_sequence.fetch_add(1) + 1;
    const auto directory = std::filesystem::path(Constants::profiling_snapshot_dir(snapshot_tag));
    result = write_snapshot_bundle_atomic(directory, Constants::my_hostname,
                                          static_cast<uint64_t>(getpid()), sequence,
                                          snapshot_tag.empty() ? "final" : snapshot_tag,
                                          snapshot, Operations::config());
    if(!result.success) LOG(ERROR) << "failed to write profiling report bundle: " << result.error << std::endl;
    return result;
}

void Profiling::reset() {
    Operations::snapshot_and_reset();
}
