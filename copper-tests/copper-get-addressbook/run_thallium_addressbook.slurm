#!/bin/bash -x
#SBATCH -A UMS046
#SBATCH -J thallium_addrbook
#SBATCH -o %x-%j.out
#SBATCH -t 00:10:00
#SBATCH -p batch
#SBATCH -N 2

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${BUILD_DIR:-${TESTS_DIR}/build}"
OUTDIR="${OUTDIR:-${SCRIPT_DIR}}"

cd "${SCRIPT_DIR}"
echo "job_id=${SLURM_JOB_ID}"
echo "nodes=${SLURM_JOB_NUM_NODES}"
echo "nodelist=${SLURM_NODELIST}"

if [ ! -x "${BUILD_DIR}/list_cxi_hsn_thallium" ]; then
    "${SCRIPT_DIR}/compile_thallium_addressbook.sh"
fi

OUTFILE="${OUTDIR}/thallium-addressbook-${SLURM_JOB_ID}.out"
srun -N "${SLURM_NNODES}" --ntasks-per-node=1 "${BUILD_DIR}/list_cxi_hsn_thallium" > "${OUTFILE}"
cat "${OUTFILE}"
