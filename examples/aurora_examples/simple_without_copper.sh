#!/bin/bash -x
#PBS -l select=2
#PBS -l walltime=01:00:00
#PBS -A datascience
#PBS -q prod
#PBS -k doe
#PBS -l filesystems=flare

# Example Aurora PBS workflow without Copper.

cd "${PBS_O_WORKDIR}"
echo "Job ID: ${PBS_JOBID}"
echo "Nodes:"
cat "${PBS_NODEFILE}"

NNODES=$(wc -l < "${PBS_NODEFILE}")
RANKS_PER_NODE=12
NRANKS=$((NNODES * RANKS_PER_NODE))

APP_BASE=${APP_BASE:-/lus/flare/projects/datascience/${USER}/exp1}

time mpirun --np "${NRANKS}" --ppn "${RANKS_PER_NODE}" \
  --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
  --genv=PYTHONPATH="${APP_BASE}/lus_custom_pip_env:${PYTHONPATH}" \
  python3 -c "import torch; print(torch.__file__)"
