#!/bin/bash -x
# Generic Frontier Slurm workflow with Copper.

#SBATCH -A UMS046
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 00:15:00
#SBATCH -p batch
#SBATCH -q debug
#SBATCH -N 2
#SBATCH -C nvme
# sbatch ./simple_with_copper.sh
# salloc -p batch -q debug -A UMS046 -J copper_spindle -t 01:00:00 -C nvme -N 2

module load ums ums046 copper
module list

APP_BASE=${APP_BASE:-/lustre/orion/proj-shared/ums046/${USER}/exp1}
LOGDIR=${LOGDIR:-${APP_BASE}/copper_logs}
COPPER_MOUNT_PATH=${COPPER_MOUNT_PATH:-/mnt/bb/$USER/copper_mount}

launch_copper.sh -d "${LOGDIR}" -v "${COPPER_MOUNT_PATH}"
# For a personal Conda environment, prepend only the activate path with
# $COPPER_MOUNT_PATH (for example, /tmp/${USER}/copper/.../conda_env).
conda activate "${COPPER_MOUNT_PATH}${APP_BASE}/conda_env"
CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73

/usr/bin/time srun --overlap -N "${SLURM_NNODES}" -n $((SLURM_NNODES * 8)) \
  --ntasks-per-node=8 --cpu-bind="${CPU_BINDING_MAP}" \
  python3 -c "import torch; print('torch imported from:', torch.__file__)"
 
conda deactivate
stop_copper.sh -d "${LOGDIR}" -v "${COPPER_MOUNT_PATH}"
