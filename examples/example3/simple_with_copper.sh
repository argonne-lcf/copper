#!/bin/bash -x
#SBATCH -A GEN008
#SBATCH -J copper_spindle
#SBATCH -o %x-%j.out
#SBATCH -t 00:30:00
#SBATCH -p batch
#SBATCH -N 2
#SBATCH -q debug
# sbatch ./simple_with_copper.sh
# salloc -A GEN008 -J copper_spindle -t 00:30:00 -q debug -N 2 

# This example shows loading python modules from a lustre directory with using copper.
 
cd $SLURM_SUBMIT_DIR
echo Jobid: $SLURM_JOBID
echo Running on nodes $SLURM_NODELIST

# module load copper 
# launch_copper.sh
# Prepend /tmp/${USER}/copper/ to all your absolute paths if you want your I/O to go through copper (including PYTHON_PATH, CONDA_PREFIX, CONDA_ROOT and PATH)


RANKS_PER_NODE=8
echo "App running on NUM_OF_NODES=${SLURM_JOB_NUM_NODES}  RANKS_PER_NODE=${RANKS_PER_NODE} "
module load cray-python

# . /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
# spack env activate kaushik_env_1 
# spack load mochi-margo

#  srun -N ${SLURM_JOB_NUM_NODES} --ntasks-per-node=${RANKS_PER_NODE} --network=single_node_vni,job_vni ./a.out 

# The below 3 lines are only for first time setup to install a package on a custom dir. Do not use in this job script.
# module load frameworks
# mkdir -p /lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env
# pip install --target=/lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env numpy 


time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:1:9:17:25:33:41:49:57 --genvall \
            --genv=PYTHONPATH=/tmp/${USER}/lustre/orion/gen008/proj-shared/kaushik/gitrepos/lus_pip_numpy_env \
            python3 -c "import numpy; print(numpy.__file__)"

stop_copper.sh

# srun -N ${NNODES} -n ${RANKS_PER_NODE} -c1   --export=PYTHONPATH=/lustre/orion/gen008/proj-shared/kaushik/gitrepos/lus_pip_numpy_env/:$PYTHONPATH python -c "import numpy; print(numpy.__file__)"
# srun -N ${NNODES} -n ${RANKS_PER_NODE} --cpu-bind=map_cpu:1,9,17,25,33,41,49,57 python3 -c "import numpy; print(numpy.__file__)"
