#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A Aurora_deployment
#PBS -q lustre_scaling
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=02:00:00 -A Aurora_deployment -l filesystems=flare -q lustre_scaling  ./withcopper_aurora_job_script.sh # or - I 

# This example shows loading python modules from a lustre directory with using copper.
 
cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

module load copper 
launch_copper.sh
# Prepend /tmp/${USER}/copper/ to all your absolute paths if you want your I/O to go through copper (including PYTHON_PATH, CONDA_PREFIX, CONDA_ROOT and PATH)

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
module load python

# The below 3 lines are only for first time setup to install a package on a custom dir. Do not use in this job script.
# module load frameworks
# mkdir -p /lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env
# pip install --target=/lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env numpy 


time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:9:14:19:20:25:56:61:66:71:74:79 --genvall \
            --genv=PYTHONPATH=/tmp/${USER}/copper/lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env \
            python3 -c "import numpy; print(numpy.__file__)"

stop_copper.sh