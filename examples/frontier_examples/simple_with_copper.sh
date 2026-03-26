#!/bin/bash -x
#SBATCH -A UMS046
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 00:15:00
#SBATCH -p batch
#SBATCH -q debug
#SBATCH -N 2
#SBATCH -C nvme

# Example Frontier Slurm workflow with Copper enabled.

module load ums ums046 copper
module list
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

APP_BASE=${APP_BASE:-/lustre/orion/proj-shared/ums046/${USER}/exp1}
MY_COPPER_MOUNT=${MY_COPPER_MOUNT:-/mnt/bb/${USER}/copper_mount}
LOGDIR=${LOGDIR:-${APP_BASE}/copper-logs-dir}

"${SCRIPT_DIR}/launch_copper.sh" -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}" -n "cxi://cxi1" -b "1,2"

# For a personal Conda environment, prepending only the activate path with the
# Copper mount is usually sufficient.
conda activate "${MY_COPPER_MOUNT}${APP_BASE}/conda_env"
CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73

/usr/bin/time srun --overlap -N "${SLURM_NNODES}" -n $((SLURM_NNODES * 8)) \
  --ntasks-per-node=8 --cpu-bind="${CPU_BINDING_MAP}" \
  python3 -c "import torch; print('torch imported from:', torch.__file__)"

"${SCRIPT_DIR}/stop_copper.sh" -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"
