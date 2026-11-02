#include "startup/startup_tools.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    using namespace CopperStartup;

    CHECK(parse_log_level("0", "startup") == 0);
    CHECK(parse_log_level("5", "operations") == 5);
    try {
        (void)parse_log_level("6", "startup");
        CHECK(false);
    } catch(const std::invalid_argument&) {}
    try {
        (void)parse_log_level("debug", "operation");
        CHECK(false);
    } catch(const std::invalid_argument&) {}
    try {
        (void)parse_positive_timeout_ms("0", "provider readiness");
        CHECK(false);
    } catch(const std::invalid_argument&) {}
    try {
        (void)parse_positive_timeout_ms("25ms", "engine startup");
        CHECK(false);
    } catch(const std::invalid_argument&) {}
    CHECK(parse_positive_timeout_ms("250", "provider readiness") == 250);

    using Clock = std::chrono::steady_clock;
    auto now = Clock::time_point{};
    std::vector<std::chrono::milliseconds> sleeps;
    int probes = 0;
    ReadinessHooks hooks{
        [&] { return ++probes == 3; },
        [&] { return now; },
        [&](std::chrono::milliseconds delay) { sleeps.push_back(delay); now += delay; }
    };
    const auto ready = wait_until_ready(hooks, std::chrono::milliseconds(100),
                                        std::chrono::milliseconds(10), std::chrono::milliseconds(40));
    CHECK(ready.ready);
    CHECK(ready.attempts == 3);
    CHECK(sleeps.size() == 2);
    CHECK(sleeps[0].count() == 10);
    CHECK(sleeps[1].count() == 20);

    now = Clock::time_point{};
    probes = 0;
    sleeps.clear();
    hooks.probe = [&] { ++probes; return false; };
    const auto never = wait_until_ready(hooks, std::chrono::milliseconds(25),
                                        std::chrono::milliseconds(10), std::chrono::milliseconds(20));
    CHECK(!never.ready);
    CHECK(never.timed_out);
    CHECK(never.attempts == 3);
    CHECK(now == Clock::time_point{} + std::chrono::milliseconds(25));

    hooks.probe = []() -> bool { throw std::runtime_error("CXI init error"); };
    const auto failed = wait_until_ready(hooks, std::chrono::milliseconds(25),
                                         std::chrono::milliseconds(10), std::chrono::milliseconds(20));
    CHECK(!failed.ready);
    CHECK(!failed.timed_out);
    CHECK(failed.error == "CXI init error");

    const auto temp = std::filesystem::temp_directory_path() / "copper-startup-tools-test.jsonl";
    std::filesystem::remove(temp);
    StartupTelemetry telemetry(temp.string(), "host-a", 42);
    telemetry.record("engine_construct", PhaseState::begin);
    telemetry.record("engine_construct", PhaseState::complete, "ofi+cxi://0x123");
    telemetry.record("provider_registration", PhaseState::failed, "CXI init error");
    std::ifstream input(temp);
    std::string line;
    std::vector<std::string> lines;
    while(std::getline(input, line)) lines.push_back(line);
    CHECK(lines.size() == 3);
    CHECK(lines[0].find("\"host\":\"host-a\"") != std::string::npos);
    CHECK(lines[0].find("\"pid\":42") != std::string::npos);
    CHECK(lines[0].find("\"phase\":\"engine_construct\"") != std::string::npos);
    CHECK(lines[0].find("\"state\":\"begin\"") != std::string::npos);
    CHECK(lines[1].find("\"elapsed_us\":") != std::string::npos);
    CHECK(lines[1].find("ofi+cxi://0x123") != std::string::npos);
    CHECK(lines[2].find("CXI init error") != std::string::npos);
    std::filesystem::remove(temp);

    CHECK(category_enabled(LogCategory::startup, 5, 0, 5));
    CHECK(!category_enabled(LogCategory::operation, 5, 0, 5));
    CHECK(category_enabled(LogCategory::operation, 5, 5, 5));

    // The production CLI must support help without requiring mount arguments.
    // This is intentionally a behavioral contract rather than a source-string check.
    CHECK(std::filesystem::exists("./cu_fuse"));
    const int help_status = std::system("./cu_fuse --help > cu_fuse-help.out 2>&1");
    CHECK(help_status == 0);
    std::ifstream help_input("cu_fuse-help.out");
    const std::string help_text((std::istreambuf_iterator<char>(help_input)),
                                std::istreambuf_iterator<char>());
    CHECK(help_text.find("-vpath") != std::string::npos);
    std::filesystem::remove("cu_fuse-help.out");
    return 0;
}
