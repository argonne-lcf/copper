#!/bin/bash
# Quick command note for the minimal RPC example.
# Expected usage after the site environment is already loaded:
#   cd .../copper-tests
#   ./build.sh
#   srun -N 1 --ntasks-per-node=1 build/server_rpc &
#   build/client_rpc <server_addr> <bytes>

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${BUILD_DIR:-${TESTS_DIR}/build}"

cat <<EOF
Build commands:
  cd ${TESTS_DIR}
  ./build.sh

Example server run:
  srun -N 1 --ntasks-per-node=1 ${BUILD_DIR}/server_rpc

Example client run:
  ${BUILD_DIR}/client_rpc <server_addr> <bytes>
EOF
