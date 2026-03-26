#!/bin/bash
# Build the standalone copper-tests tree.
# Expected usage:
#   1. load the site compiler/MPI/Mochi environment
#   2. run `./build.sh`
# Optional environment overrides:
#   BUILD_TYPE=Debug
#   BUILD_DIR=/tmp/copper-tests-build
#   CMAKE_ARGS="-DCMAKE_VERBOSE_MAKEFILE=ON"

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SOURCE_DIR="${SCRIPT_DIR}"
BUILD_DIR="${BUILD_DIR:-${SOURCE_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

echo "source_dir=${SOURCE_DIR}"
echo "build_dir=${BUILD_DIR}"
echo "build_type=${BUILD_TYPE}"

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${CMAKE_ARGS:-}

cmake --build "${BUILD_DIR}" -j "${BUILD_JOBS:-4}"

echo "Built copper-tests into ${BUILD_DIR}"
