#!/bin/bash -x
#PBS -l select=1024
#PBS -l walltime=01:00:00
#PBS -A Aurora_deployment
#PBS -q lustre_scaling
#PBS -k doe

# qsub -l select=1024:ncpus=208 -l walltime=02:00:00 -A Aurora_deployment -l filesystems=flare -q lustre_scaling  ./pbs or - I 

export TZ='/usr/share/zoneinfo/US/Central'

date 

# . /lus/flare/projects/Aurora_deployment/kaushik/copper/gitrepos/git-spack-repo/spack/share/spack/setup-env.sh
# spack env activate kaushik_env_1


cd $PBS_O_WORKDIR

echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1         # Number of MPI ranks per node
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
# CPU_BINDING1=list:4
EXT_ENV="--env FI_CXI_DEFAULT_CQ_SIZE=1048576"

mpirun --cpu-bind=core   ${EXT_ENV} -np ${NRANKS} -ppn ${RANKS_PER_NODE}  /lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/scripts/filesystem/mnt_fs.sh &
# mpirun $CPU_BINDING1   ${EXT_ENV} -np ${NRANKS} -ppn ${RANKS_PER_NODE}  /lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/scripts/filesystem/mnt_fs.sh &
sleep 100s 

RANKS_PER_NODE=12         # Number of MPI ranks per node
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

mpirun   ${EXT_ENV} -np ${NRANKS} -ppn ${RANKS_PER_NODE}  /lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/scripts/test/run_tests_view.sh

# mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view
# mpirun -np 1 -ppn 1 build/rpc_shutdown


# set the following in 2 terminals
# echo $PBS_JOBID
# echo $PBS_NODEFILE
# export PBS_JOBID=699852.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov
# export PBS_NODEFILE=/var/spool/pbs/aux/699852.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov