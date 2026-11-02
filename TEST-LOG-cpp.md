# C++ startup tools strict vertical TDD log

## Slice 1 — option validation and readiness failure-injection abstraction

### RED
Command:
`c++ -std=c++20 -Isrc tests/test_startup_tools.cpp -o /tmp/test_startup_tools`

Exit: `1`

Observed expected failure:
`fatal error: startup/startup_tools.h: No such file or directory`

The wished-for dependency-light startup API did not exist.

### GREEN
Command:
`c++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_startup_tools.cpp -o /tmp/test_startup_tools && (cd build && /tmp/test_startup_tools)`

Exit: `0`

Validated strict integer/range parsing and deterministic delayed-ready, never-ready timeout, and injected CXI-error paths without Thallium/FUSE dependencies.

## Slice 2 — durable phase telemetry and independent log categories

### RED
Command:
`c++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_startup_tools.cpp -o /tmp/test_startup_tools`

Exit: `1`

Observed expected failures included:
- `StartupTelemetry was not declared in this scope`
- `PhaseState has not been declared`
- `LogCategory has not been declared`
- `category_enabled was not declared in this scope`

### GREEN
Command:
`c++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_startup_tools.cpp -o /tmp/test_startup_tools && (cd build && /tmp/test_startup_tools)`

Exit: `0`

Validated atomic temp+rename JSONL telemetry with host/PID/phase/state/elapsed/detail fields and independent startup/transport versus operation category decisions.

## Final dependency-light regression

Command:
`cmake -S . -B build-cpp-light -DCOPPER_BUILD_RUNTIME=OFF -DBUILD_TESTING=ON && cmake --build build-cpp-light -j2 && ctest --test-dir build-cpp-light --output-on-failure`

Final exit: `0`

Result: dependency-light matrix passed 11/11 enabled tests; `startup_tools` is intentionally disabled when `COPPER_BUILD_RUNTIME=OFF` because its behavioral `cu_fuse --help` assertion requires the production binary. The full Aurora runtime build runs and passes all 12/12 tests, including `startup_tools`.

Additional check: `git diff --check` exit `0`.
