#!/bin/bash -x
#SBATCH -A GEN008
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 02:00:00
#SBATCH -p batch
#SBATCH -N 1024
#SBATCH --network=single_node_vni,job_vni,def_tles=0
#SBATCH --threads-per-core=2  

# sbatch ./simple_with_copper.sh
# salloc -A GEN008 -J copper_spindle -t 00:30:00 -q debug -N 2 
# salloc --network=single_node_vni,job_vni,def_tles=0 --threads-per-core=2   --exclusive   -A GEN008 -J copper_spindle -t 00:30:00 -q debug -N 2
# This example shows loading python modules from a lustre directory with using copper.
 
echo Jobid: $SLURM_JOBID
echo Running on nodes $SLURM_NODELIST
RANKS_PER_NODE=1
echo "App running on NUM_OF_NODES=${SLURM_JOB_NUM_NODES}  RANKS_PER_NODE=${RANKS_PER_NODE} "
module load cray-python
cd /lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/examples/example3
sh ./launch_copper.sh
sleep 100s
export PYTHONPATH=/tmp/kaushikv/copper/lustre/orion/gen008/proj-shared/kaushik/experiments/torch-without-copper/pip_dirs/lus_pip_torch_env_1:$PYTHONPATH

time srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE --cpus-per-task=1  --threads-per-core=1 --network=single_node_vni,job_vni    python3 -c "import torch; print(torch.__file__)"

time srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE --cpus-per-task=1  --threads-per-core=1 --network=single_node_vni,job_vni    python3 -c "import torch; print(torch.__file__)"


# --cpu-bind=map_cpu:5,13,21,29,37,45,53,61
