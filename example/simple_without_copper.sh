#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A datascience
#PBS -q prod
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=01:00:00 -A datascience -l filesystems=home:flare -q prod  ./simple_without_copper.sh # or - I 

# This example shows loading python modules from a lustre directory without using copper.
 
cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

module load frameworks

# The below line is required only for the first time setup to install a package on a custom directory. 
# python -m pip install  --target=/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/ dragonhpc


time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
            --genv=PYTHONPATH=/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/:$PYTHONPATH \
             python3 -c "import dragon; print(dragon.__file__)"
