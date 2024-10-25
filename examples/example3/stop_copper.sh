#!/bin/bash -x

echo "Stopping Copper Gracefully On All Nodes : Start" 
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper 
clush --hostfile ${PBS_NODEFILE} "pkill -9 cu_fuse"
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
echo "Stopping Copper Gracefully On All Nodes : End"
