#!/bin/bash -x

module load copper 
CUPATH=$COPPER_ROOT/bin/cu_fuse

rm -rf ~/copper_logs*
LOGDIR=~/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}
mkdir -p ${LOGDIR} #only on head node

CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
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
