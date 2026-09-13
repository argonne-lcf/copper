# Modern CMake and dependency-light CTest

## Problem
The old minimum predated modern target/test behavior and no local tests existed; redundant-RPC behavior was an implicit variable.

## Design
Require CMake 3.16, declare `BLOCK_REDUNDANT_RPCS` as an ON/OFF option, enable CTest, and add dependency-light C++/Python tests.

## Tests
The dependency-light tests pass locally with Release-style `-DNDEBUG -Wall -Wextra -Werror`. On Aurora, clean runtime builds with `BLOCK_REDUNDANT_RPCS=OFF` and `ON` both succeeded and each CTest configuration passed 5/5 tests.

## Limitations
The production target still requires MPI, Margo, Thallium, Cereal, Mercury, and FUSE.
