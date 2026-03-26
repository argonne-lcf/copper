#!/bin/bash
# Build only the address-book discovery helper from the local version9 tree.
# Expected usage after the site environment is already loaded:
#   ./compile_thallium_addressbook.sh

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${BUILD_DIR:-${TESTS_DIR}/build}"

echo "tests_dir=${TESTS_DIR}"
echo "build_dir=${BUILD_DIR}"

cmake -S "${TESTS_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${CMAKE_ARGS:-}

cmake --build "${BUILD_DIR}" --target list_cxi_hsn_thallium -j "${BUILD_JOBS:-4}"

echo "Built ${BUILD_DIR}/list_cxi_hsn_thallium"
