#!/bin/bash -x
# qsub -l nodes=100 -q workq  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment 
# qsub -l select=7 -q workq  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment -I


#PBS -A Aurora_deployment
#PBS -k doe

module use /soft/modulefiles
module load frameworks/2023.12.15.001 
module list
. /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
which python
which spack
spack find 

cd /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/cppr-kau/overlay/stage5


cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
EXT_ENV="--env FI_CXI_DEFAULT_CQ_SIZE=1048576 " 
which python




# for NNODES in 1 2 4 8 16 32 64 96 100
# do
for BUF_SIZE in 1 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728 268435456 536870912 1073741824 
do  
    # NRANKS=$(( NNODES * RANKS_PER_NODE ))
    echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE} BUF_SIZE= ${BUF_SIZE}"
    
    date 
    rm copper_address_book.txt 
    sleep 200s && pkill -f copper & 
    mpirun -np ${NRANKS} -ppn ${RANKS_PER_NODE} ./copper ${BUF_SIZE} 
    date 
    sleep 10s
    
    
# done
done
