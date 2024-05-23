#!/bin/bash -x
# qsub -l nodes=100 -q workq  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment 
# qsub -l select=7 -q workq  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment -I

# pbsnodes -av | grep -Ei " Mom =|state = free|resources_available.validation = False" >del1.txt
# qsub -l select=host=x1922c6s1b0n0+1:host=x1922c6s2b0n0+1:host=x1922c6s3b0n0+1:host=x1922c6s4b0n0+1:host=x1922c6s5b0n0 -q diag  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment -I 
#PBS -A Aurora_deployment
#PBS -k doe

module use /soft/preview-modulefiles/24.086.0
module load frameworks/2024.04.15.002
module list
. /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
which python
which spack
spack find 


cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
EXT_ENV="--env FI_CXI_DEFAULT_CQ_SIZE=1048576 " 
which python


for BUF_SIZE in 1 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728 268435456 536870912 1073741824 
do  
    echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE} BUF_SIZE= ${BUF_SIZE}"
    date 
    rm copper_address_book.txt 
    # ./shutdown 200 &

    if [ $BUF_SIZE -lt 33554432 ]
    then
           ./shutdown 60 &
    else
           ./shutdown 120 &
    fi



    mpirun --env MARGO_ENABLE_MONITORING=1 --env MARGO_MONITORING_FILENAME_PREFIX="${PBS_JOBID}_${NNODES}_${NRANKS}_${RANKS_PER_NODE}_${BUF_SIZE}_" -np ${NRANKS} -ppn ${RANKS_PER_NODE} ./copper ${BUF_SIZE} 
    date 
    sleep 5s
done

# mpirun --env MARGO_ENABLE_MONITORING=1 --env MARGO_MONITORING_FILENAME_PREFIX="${PBS_JOBID}_${NNODES}_${NRANKS}_${RANKS_PER_NODE}_${BUF_SIZE}_" -np 5 -ppn 1 ./copper 4

