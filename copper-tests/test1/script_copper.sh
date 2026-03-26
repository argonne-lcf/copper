#!/bin/bash -x
# Example PBS wrapper for the test2 forwarding benchmark.
# Expected usage:
# 1. load the site compiler/MPI/Mochi environment
# 2. `qsub script_copper.sh`
#
# The script uses locally built binaries from `../build`.

#PBS -A Aurora_deployment
#PBS -k doe

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${BUILD_DIR:-${TESTS_DIR}/build}"

cd "${PBS_O_WORKDIR:-$SCRIPT_DIR}"
echo "Jobid: ${PBS_JOBID}"
echo "Running on nodes:"
cat "${PBS_NODEFILE}"

NNODES=$(wc -l < "${PBS_NODEFILE}")
RANKS_PER_NODE=1
NRANKS=$((NNODES * RANKS_PER_NODE))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

if [ ! -x "${BUILD_DIR}/copper" ] || [ ! -x "${BUILD_DIR}/shutdown" ]; then
    "${TESTS_DIR}/build.sh"
fi

for BUF_SIZE in 1 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728 268435456 536870912 1073741824
do
    echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE} BUF_SIZE=${BUF_SIZE}"
    date
    rm -f copper_address_book.txt

    if [ "${BUF_SIZE}" -lt 33554432 ]; then
        "${BUILD_DIR}/shutdown" 60 &
    else
        "${BUILD_DIR}/shutdown" 120 &
    fi

    mpirun --env MARGO_ENABLE_MONITORING=1 \
        --env MARGO_MONITORING_FILENAME_PREFIX="${PBS_JOBID}_${NNODES}_${NRANKS}_${RANKS_PER_NODE}_${BUF_SIZE}_" \
        -np "${NRANKS}" \
        -ppn "${RANKS_PER_NODE}" \
        "${BUILD_DIR}/copper" "${BUF_SIZE}"
    date
    sleep 5s
done
