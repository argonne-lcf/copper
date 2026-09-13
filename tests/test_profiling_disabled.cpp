#include "fs/constants.h"
#include "metric/operations.h"
#include "metric/profiling.h"

#include <filesystem>
#include <iostream>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    const auto root = std::filesystem::temp_directory_path() / "copper-disabled-profiler";
    std::filesystem::remove_all(root);
    Constants::log_output_dir = root.string();
    Constants::profile_metrics = false;
    Constants::profile_top_n = 0;
    Constants::profile_paths_full = false;
    Operations::snapshot_and_reset();
    Operations::complete(OperationFunction::read, 9, ProfileOutcome::success);
    const auto result = Profiling::write_reports("unused");
    CHECK(!result.success);
    CHECK(result.error == "profiling disabled");
    CHECK(Operations::snapshot().operations[static_cast<size_t>(OperationFunction::read)].count == 0);
    CHECK(!std::filesystem::exists(root));
    return 0;
}