#!/bin/bash -x
# set -e  # Exit on error
# set -x  # Print each command before execution

echo "Launching Copper Gracefully On All Nodes : Start" 

log_level=6
log_type="file"
trees=1
max_cacheable_byte_size=$((500*1024*1024))
sleeptime=20
LOGDIR=~/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}
rm -rf ~/copper_logs*
# COPPER_ROOT=/lus/flare/projects/Aurora_deployment/kaushik/copper/copper-aurora/copper/build
# CUPATH=$COPPER_ROOT/cu_fuse
CUPATH=$COPPER_ROOT/bin/cu_fuse
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
physcpubind="48-51"
facility_address_book=/opt/clmgr/etc/copper/aurora_copper_addressbook.txt

while getopts "l:t:T:M:s:b:F:" opt; do
  case ${opt} in
    l ) log_level=$OPTARG ;;
    t ) log_type=$OPTARG ;;
    T ) trees=$OPTARG ;;
    M ) max_cacheable_byte_size=$OPTARG ;;
    s ) sleeptime=$OPTARG ;;
    b ) physcpubind=$OPTARG ;;
    F ) facility_address_book=$OPTARG ;;
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


mkdir -p "${LOGDIR}" #only on head node
clush --hostfile "${PBS_NODEFILE}" "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${PBS_NODEFILE}" "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${PBS_NODEFILE}" "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" # on all compute nodes

read -r -d '' CMD << EOM
   numactl --physcpubind=${physcpubind}
   $CUPATH
     -tpath /
     -vpath ${CU_FUSE_MNT_VIEWDIR}
     -log_level ${log_level}
     -log_type ${log_type}
     -log_output_dir ${LOGDIR}
     -net_type cxi 
     -trees ${trees} 
     -nf ${PBS_NODEFILE}
     -facility_address_book ${facility_address_book}
     -max_cacheable_byte_size ${max_cacheable_byte_size}
     -s ${CU_FUSE_MNT_VIEWDIR}
EOM

clush --hostfile "${PBS_NODEFILE}" $CMD
sleep "${sleeptime}"s # add 60s if you are running on more than 2k nodes

echo "Launching Copper Gracefully On All Nodes : End" 
