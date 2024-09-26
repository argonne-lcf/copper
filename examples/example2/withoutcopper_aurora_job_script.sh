#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A Aurora_deployment
#PBS -q lustre_scaling
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=02:00:00 -A Aurora_deployment -l filesystems=flare -q lustre_scaling  ./withoutcopper_aurora_job_script.sh # or - I 

# This example shows loading python modules from a lustre directory (the standard way) without using copper.

cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`
NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

module use /lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024 # This is the location of the cloned copy of your custom conda environment on lustre
module load frameworks/2024.1 # This will start your conda environment at /lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024
which python

time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:9:14:19:20:25:56:61:66:71:74:79 --genvall \
            --genv=PYTHONPATH=/lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024 \
            python3 real_app.py
