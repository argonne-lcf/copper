#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A Aurora_deployment
#PBS -q lustre_scaling
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=02:00:00 -A Aurora_deployment -l filesystems=flare -q lustre_scaling  ./pbs-script.sh or - I 

# This example shows loading python modules from a lustre directory with using copper.

export TZ='/usr/share/zoneinfo/US/Central'
date 
cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`


# starting copper section 
NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "Copper running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
CUPATH=/lus/flare/projects/Aurora_deployment/kaushik/copper-spack-recipe/gitrepos/copper/build
LOGDIR=~/copper-logs/${PBS_JOBID}
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
rm -rf ~/copper_logs*
mkdir -p ${LOGDIR}
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "mkdir -p ${CU_FUSE_MNT_VIEWDIR}"

export FI_MR_ZE_CACHE_MONITOR_ENABLED=0
export FI_MR_CACHE_MONITOR=disabled
export FI_CXI_RX_MATCH_MODE=hybrid
export FI_CXI_DEFAULT_CQ_SIZE=1048576
export FI_CXI_CQ_FILL_PERCENT=30
export MPI_PROVIDER=$FI_PROVIDER
unset MPIR_CVAR_CH4_COLL_SELECTION_TUNING_JSON_FILE
unset MPIR_CVAR_COLL_SELECTION_TUNING_JSON_FILE
export PALS_PING_PERIOD=240
export PALS_RPC_TIMEOUT=240

read -r -d '' CMD << EOM
   numactl --physcpubind="0-3"
   $CUPATH/cu_fuse 
     -tpath /
     -vpath ${CU_FUSE_MNT_VIEWDIR}
     -log_level 6
     -log_type file
     -log_output_dir ${LOGDIR}
     -net_type cxi 
     -trees 1 
     -nf ${PBS_NODEFILE}
     -max_cacheable_byte_size $((10*1024*1024))
     -s ${CU_FUSE_MNT_VIEWDIR}
EOM
# check with and without -f - running cu fuse on foreground vs background
clush --hostfile ${PBS_NODEFILE} $CMD
sleep 120s # Preferred to give sometime for copper service to be started on all nodes. 
ls ${CU_FUSE_MNT_VIEWDIR}



# App section
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
module use /lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024
module load frameworks/2024.1
conda deactivate
conda activate ${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024
which python
CPU_BINDING=list:4:9:14:19:20:25:56:61:66:71:74:79 

date
time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=${CPU_BINDING} --genvall --genv=PYTHONPATH=${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024  python3 real_app.py
date

# clean up copper
conda deactivate
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
export UID=$(id -u $USER)
clush --hostfile ${PBS_NODEFILE} "pkill -U $UID"
