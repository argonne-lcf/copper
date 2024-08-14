#!/bin/bash -x
# qsub -l nodes=2:ncpus=208 -q workq  -l walltime=02:00:00 -l filesystems=gila -A  Aurora_deployment ./pbs_job_

module use  /home/jmitche1/anl_release/aurora/2024/q3
module load frameworks_2024_8.lua
module list

export CCL_KVS_MODE=mpi
export CCL_CONFIGURATION_PATH=""
export CCL_CONFIGURATION=cpu_gpu_dpcpp
export CCL_ROOT="/flare/Aurora_deployment/sriraj/kvs2/build_2024.2.0/_install"
export LD_LIBRARY_PATH=/flare/Aurora_deployment/sriraj/kvs2/build_2024.2.0/_install/lib/:$LD_LIBRARY_PATH
export CPATH=/flare/Aurora_deployment/sriraj/kvs2/build_2024.2.0/_install/include/:$CPATH
export LIBRARY_PATH=/flare/Aurora_deployment/sriraj/kvs2/build_2024.2.0/_install/lib/:$LIBRARY_PATH
export CCL_KVS_CONNECTION_TIMEOUT=3600

cd $PBS_O_WORKDIR

CUPATH=/home/harms/working/copper/copper/build
LOGDIR=/home/harms/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}

mkdir -p ${LOGDIR}
WCOLL=$PBS_NODEFILE pdsh -t 60 fusermount3 -u /tmp/copper
WCOLL=$PBS_NODEFILE pdsh mkdir -p /tmp/copper
read -r -d '' CMD << EOM
   numactl --physcpubind="0-3"
   $CUPATH/cu_fuse
     -tpath /
     -vpath /tmp/copper
     -log_level 4
     -log_type file
     -log_output_dir ${LOGDIR}
     -es 1
     -nf ${PBS_NODEFILE}
     -max_cacheable_byte_size $((10*1024*1024))
     -s /tmp/copper
EOM
WCOLL=$PBS_NODEFILE pdsh -t 60 $CMD

sleep 10

echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`
NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12          # Number of MPI ranks per node
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
CPU_BINDING1=list:4:9:14:19:20:25:56:61:66:71:74:79
EXT_ENV="--env FI_CXI_DEFAULT_CQ_SIZE=1048576"

which python
mkdir -p ./out$PBS_JOBID/profiling_app_py_torch_ccl_gpu

IFS=':' read -ra PATHS <<< $PYTHONPATH
CPPRPATH=${PATHS[@]/\//\/tmp/copper/}
CPPRPATH=${CPPRPATH/ /:}
CPPRPATH=${CPPRPATH/3.9/3.10}


# Note for each BUF_SIZE, the dimension of the tensor will be BUF_SIZE/2, BUF_SIZE/2, resulting true all reduce buffer will be BUF_SIZE/2 * BUF_SIZE/2. 
# For example, BUF_SIZE=8, dim1=4 dim2=4, all reduce buffer=4*4=16 

#for BUF_SIZE in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216  33554432 67108864 134217728 268435456
for BUF_SIZE in 1 2 4 8 16 32 64
do
    date
    echo ${BUF_SIZE}

        mpiexec ${EXT_ENV} --np ${NRANKS} -ppn ${RANKS_PER_NODE}  --cpu-bind  $CPU_BINDING1  \
	--genvall \
	--genv=PYTHONPATH=${CPPRPATH} \
        python3 ./profiling_app_py_torch_ccl_gpu.py ${BUF_SIZE} > ./out$PBS_JOBID/profiling_app_py_torch_ccl_gpu/${PBS_JOBID}_${NNODES}_${NRANKS}_${RANKS_PER_NODE}_${BUF_SIZE}_py_ccl_profiling_out.txt
    date
    echo ${BUF_SIZE}
done

WCOLL=$PBS_NODEFILE pdsh -t 60 fusermount3 -u /tmp/copper
