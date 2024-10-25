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

# starting copper section 

module load copper 
CUPATH=$COPPER_ROOT/bin/cu_fuse # If you are building copper on your own, set this path to your cu_fuse binary
LOGDIR=~/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}
mkdir -p ${LOGDIR} #only on head node
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
clush --hostfile ${PBS_NODEFILE} "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" # on all compute nodes

read -r -d '' CMD << EOM
   numactl --physcpubind="0-3"
   $CUPATH
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

clush --hostfile ${PBS_NODEFILE} $CMD 
sleep 20s # add 60s if you are running on more than 2k nodes

# end copper section


NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

# The below 3 lines are only for first time setup to install a package on a custom dir. Do not use in this job script.
# module load frameworks
# mkdir -p /lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env
# pip install --target=/lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env numpy 


time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:9:14:19:20:25:56:61:66:71:74:79 --genvall \
            --genv=PYTHONPATH=${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/kaushik/copper/oct24/copper/run/copper_conda_env \
            python3 -c "import numpy; print(numpy.__file__)"


#Stopping copper
clush --hostfile ${PBS_NODEFILE} "pkill -9 cu_fuse"
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u /tmp/kaushikvelusamy/copper"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
