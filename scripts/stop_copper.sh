#!/bin/bash -x
rm $SLURM_NODEFILE
scontrol show hostnames > $SLURM_NODEFILE
echo "Stopping Copper Gracefully On All Nodes : Start" 
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper 
clush --hostfile ${SLURM_NODEFILE} "pkill -9 cu_fuse"
clush --hostfile ${SLURM_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${SLURM_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
echo "Stopping Copper Gracefully On All Nodes : End"
