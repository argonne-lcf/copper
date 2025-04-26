#!/bin/bash -x
#PBS -l select=10624:ncpus=208
#PBS -l walltime=00:10:00
#PBS -A datascience
#PBS -l filesystems=home:flare
#PBS -q prod-large
#PBS -k doe

cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`
NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
module load copper 
export LD_LIBRARY_PATH=/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mochi-margo-0.19.0-xxsxe4l/lib/:/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/mercury-2.4.0-xk2hmqq/lib:/opt/aurora/24.347.0/spack/unified/0.9.1/install/linux-sles15-x86_64/oneapi-2025.0.5/argobots-1.2-dlauuec/lib/:$LD_LIBRARY_PATH
mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE}  /lus/flare/projects/datascience/kaushik/copper-tests/kmd_val_tests/margo-tests/simple-margo
