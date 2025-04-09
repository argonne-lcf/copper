#!/bin/bash -x
#SBATCH -A GEN008
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 02:00:00
#SBATCH -p batch
#SBATCH -N 1024
#SBATCH --network=single_node_vni,job_vni,def_tles=0
#SBATCH --threads-per-core=2  

# sbatch ./simple_without_copper.sh
# salloc -A GEN008 -J copper_spindle -t 00:30:00 -q debug -N 2 
# salloc --network=single_node_vni,job_vni,def_tles=0 --threads-per-core=2   --exclusive   -A GEN008 -J copper_spindle -t 00:30:00 -q debug -N 2
# This example shows loading python modules from a lustre directory without using copper.

cd $SLURM_SUBMIT_DIR
echo Jobid: $SLURM_JOBID
echo Running on nodes $SLURM_NODELIST

RANKS_PER_NODE=12
echo "App running on NUM_OF_NODES=${SLURM_JOB_NUM_NODES}  RANKS_PER_NODE=${RANKS_PER_NODE} "


# The below 2 lines are only for the first time setup to install a package on a custom dir. Do not use in this job script
# module load python
# pip install --target=/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env numpy 




module load cray-python
export PYTHONPATH=/lustre/orion/gen008/proj-shared/kaushik/experiments/torch-without-copper/pip_dirs/lus_pip_torch_env_1:$PYTHONPATH

time srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE --cpus-per-task=1  --threads-per-core=1 --network=single_node_vni,job_vni    python3 -c "import torch; print(torch.__file__)"

time srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE --cpus-per-task=1  --threads-per-core=1 --network=single_node_vni,job_vni    python3 -c "import torch; print(torch.__file__)"
