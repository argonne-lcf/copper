#!/bin/bash -x
#PBS -l select=2
#PBS -l walltime=01:00:00
#PBS -A datascience
#PBS -q prod
#PBS -k doe

# Example Aurora PBS workflow with Copper enabled.

cd "${PBS_O_WORKDIR}"
echo "Job ID: ${PBS_JOBID}"
echo "Nodes:"
cat "${PBS_NODEFILE}"

NNODES=$(wc -l < "${PBS_NODEFILE}")
RANKS_PER_NODE=12
NRANKS=$((NNODES * RANKS_PER_NODE))

module load copper
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

APP_BASE=${APP_BASE:-/lus/flare/projects/datascience/${USER}/exp1}
MY_COPPER_MOUNT=${MY_COPPER_MOUNT:-/tmp/${USER}/copper_mount}
LOGDIR=${LOGDIR:-${APP_BASE}/copper-logs-dir}

"${SCRIPT_DIR}/launch_copper.sh" -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}" -n "cxi://cxi0" -b "48,49,50,51"

# For Python package directories, prepending only the relevant PYTHONPATH entry
# is usually sufficient.
time mpirun --np "${NRANKS}" --ppn "${RANKS_PER_NODE}" \
  --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
  --genv=PYTHONPATH="${MY_COPPER_MOUNT}${APP_BASE}/lus_custom_pip_env:${PYTHONPATH}" \
  python3 -c "import torch; print(torch.__file__)"

"${SCRIPT_DIR}/stop_copper.sh" -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"
