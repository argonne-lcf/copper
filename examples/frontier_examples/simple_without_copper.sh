#!/bin/bash -x
#SBATCH -A UMS046
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 00:15:00
#SBATCH -p batch
#SBATCH -q debug
#SBATCH -N 2
#SBATCH -C nvme

# Example Frontier Slurm workflow without Copper.

module load ums ums046
module list

APP_BASE=${APP_BASE:-/lustre/orion/proj-shared/ums046/${USER}/exp1}
CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73

conda activate "${APP_BASE}/conda_env"

/usr/bin/time srun --overlap -N "${SLURM_NNODES}" -n $((SLURM_NNODES * 8)) \
  --ntasks-per-node=8 --cpu-bind="${CPU_BINDING_MAP}" \
  python3 -c "import torch; print('torch imported from:', torch.__file__)"
