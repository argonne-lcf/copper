#include "metric/profiling.h"
#include "metric/operations.h"
#include "fs/constants.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if(argc != 2) return 2;
    Constants::log_output_dir = argv[1];
    Constants::my_hostname = "reset-host";
    Constants::pid = "4242";
    Constants::profile_metrics = true;
    Constants::profile_top_n = 0;
    Constants::profile_paths_full = false;
    setenv("COPPER_RUN_ID", "reset-run", 1);
    setenv("COPPER_DAEMON_GENERATION", "reset-generation", 1);
    Operations::core().complete(0, 17, ProfileOutcome::success, 0, "/x", ProfileSource::data);
    auto report = Profiling::reset();
    if(!report.success) { std::cerr << report.error; return 1; }
    if(!std::filesystem::is_regular_file(report.manifest_path)) return 3;
    const std::filesystem::path manifest(report.manifest_path);
    if(manifest.parent_path().filename() != "final") return 4;
    std::ifstream stream(manifest);
    const std::string bytes((std::istreambuf_iterator<char>(stream)), {});
    if(bytes.find("\"kind\": \"interval\"") == std::string::npos ||
       bytes.find("\"semantics\": \"interval\"") == std::string::npos ||
       bytes.find("\"reset_epoch\": 0") == std::string::npos) return 5;
    std::cout << report.manifest_path << '\n';
    return 0;
}
