#include "metric/operations.h"
#include "fs/constants.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

int main() {
    constexpr size_t default_capacity = 1024;
    constexpr size_t maximum_capacity = 65536;
    const auto capacity = [](const char* value) {
        if(value) setenv("COPPER_PROFILE_LATENCY_SAMPLES", value, 1);
        else unsetenv("COPPER_PROFILE_LATENCY_SAMPLES");
        return Operations::config().latency_capacity;
    };

    CHECK(capacity(nullptr) == default_capacity);
    CHECK(capacity("0") == 0);
    CHECK(capacity("-1") == default_capacity);
    CHECK(capacity("12x") == default_capacity);
    CHECK(capacity("18446744073709551616") == default_capacity);
    CHECK(capacity("65536") == maximum_capacity);
    CHECK(capacity("65537") == default_capacity);

    Constants::profile_metrics = false;
    Constants::profile_top_n = 0;
    Constants::profile_paths_full = false;
    Operations::snapshot_and_reset();
    Operations::complete(OperationFunction::read, 17, ProfileOutcome::success);
    CHECK(Operations::snapshot().operations[static_cast<size_t>(OperationFunction::read)].count == 0);

    Constants::profile_metrics = true;
    Operations::complete(OperationFunction::read, 17, ProfileOutcome::success);
    CHECK(Operations::snapshot().operations[static_cast<size_t>(OperationFunction::read)].count == 1);
    return 0;
}