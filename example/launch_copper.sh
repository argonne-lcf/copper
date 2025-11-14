#!/bin/bash -x
# set -e  # Exit on error
# set -x  # Print each command before execution

NNODES=`wc -l < $PBS_NODEFILE`
if [ "$NNODES" -le 64 ]; then
  sleeptime=5
elif [ "$NNODES" -ge 65 ] && [ "$NNODES" -le 1024 ]; then
  sleeptime=10
else
  sleeptime=30
fi
echo "Launching Copper On All Nodes : Start" 

log_level=6
log_type="file"
trees=1
max_cacheable_byte_size=$((10*1024*1024))
LOGDIR=/home/${USER}/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}
rm -rf /home/${USER}/copper-logs/*
# COPPER_ROOT=/lus/flare/projects/Aurora_deployment/kaushik/copper/copper-aurora/copper/build
# CUPATH=$COPPER_ROOT/cu_fuse
CUPATH=$COPPER_ROOT/bin/cu_fuse
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
physcpubind="48-51"
facility_address_book=$COPPER_ROOT/bin/copper_aurora_addressbook.txt
net_type="cxi"

while getopts "l:t:T:M:s:b:F:n:" opt; do
  case ${opt} in
    l ) log_level=$OPTARG ;;
    t ) log_type=$OPTARG ;;
    T ) trees=$OPTARG ;;
    M ) max_cacheable_byte_size=$OPTARG ;;
    s ) sleeptime=$OPTARG ;;
    b ) physcpubind=$OPTARG ;;
    F ) facility_address_book=$OPTARG ;;
    n ) net_type=$OPTARG ;;

    \? ) echo "Usage: cmd [-l] [-t] [-T] [-M] [-s] [-b] [-F]" ;;
  esac
done

echo "log_level                  : ${log_level}"
echo "log_type                   : ${log_type}"
echo "trees                      : ${trees}"
echo "max_cacheable_byte_size    : ${max_cacheable_byte_size}"
echo "sleeptime                  : ${sleeptime}"
echo "CU_FUSE_MNT_VIEWDIR        : ${CU_FUSE_MNT_VIEWDIR}"
echo "LOGDIR                     : ${LOGDIR}"
echo "PBS_NODEFILE               : ${PBS_NODEFILE}"
echo "physcpubind                : ${physcpubind}"
echo "facility_address_book      : ${facility_address_book}"
echo "net_type                   : ${net_type}"


mkdir -p "${LOGDIR}" #only on head node
clush --hostfile "${PBS_NODEFILE}" "pkill -9 -f cu_fuse"
clush --hostfile "${PBS_NODEFILE}" "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${PBS_NODEFILE}" "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${PBS_NODEFILE}" "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" # on all compute nodes
echo "The above fusermount3: bad mount point messages and folder not found errors are expected. We are unmounting the old mount points and creating new ones"
echo "Messages below this point are critical. Please check the logs in ${LOGDIR} if you see any errors"
read -r -d '' CMD << EOM
   numactl --physcpubind=${physcpubind}
   $CUPATH
     -tpath /
     -vpath ${CU_FUSE_MNT_VIEWDIR}
     -log_level ${log_level}
     -log_type ${log_type}
     -log_output_dir ${LOGDIR}
     -net_type ${net_type}
     -trees ${trees} 
     -nf ${PBS_NODEFILE}
     -facility_address_book ${facility_address_book}
     -max_cacheable_byte_size ${max_cacheable_byte_size}
     -s ${CU_FUSE_MNT_VIEWDIR}
EOM

clush --hostfile "${PBS_NODEFILE}" $CMD
sleep "${sleeptime}"s 
echo "Launching Copper On All Nodes : End" 
