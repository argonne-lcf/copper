#!/bin/bash -x

echo "Launching Copper Gracefully On All Nodes : Start"
echo "${COPPER_ROOT}"
CUPATH=${COPPER_ROOT}/build/cu_fuse
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
facility_address_book=${COPPER_ROOT}/build/olcf_frontier_copper_addressbook.txt
log_level=0
log_dir=~/copper-logs
log_type="file"
trees=1
max_cacheable_byte_size=$((10*1024*1024))
sleeptime=10
physcpubind="1,2"           # common Frontier service cores or "1,2,65,66"
net_type="cxi://cxi1"       # common Frontier endpoint family
 
while getopts "l:t:d:T:M:s:b:F:n:v:" opt; do
  case ${opt} in
    l ) log_level=$OPTARG ;;
    t ) log_type=$OPTARG ;;
    d ) log_dir=$OPTARG ;;
    T ) trees=$OPTARG ;;
    M ) max_cacheable_byte_size=$OPTARG ;;
    s ) sleeptime=$OPTARG ;;
    b ) physcpubind=$OPTARG ;;
    F ) facility_address_book=$OPTARG ;;
    n ) net_type=$OPTARG ;;
    v ) CU_FUSE_MNT_VIEWDIR=$OPTARG ;;
    \? ) echo "Usage: cmd [-l log_level 0-5] [-t log_type] [-d log_dir_base] [-T trees] [-M max_cacheable_byte_size] [-s sleeptime] [-b physcpubind] [-F facility_address_book] [-n net_type] [-v CU_FUSE_MNT_VIEWDIR]" ;;
  esac
done

log_dir=${log_dir%/}/${SLURM_JOB_ID}

echo "log_level                  : ${log_level}"
echo "log_type                   : ${log_type}"
echo "trees                      : ${trees}"
echo "max_cacheable_byte_size    : ${max_cacheable_byte_size}"
echo "sleeptime                  : ${sleeptime}"
echo "CU_FUSE_MNT_VIEWDIR        : ${CU_FUSE_MNT_VIEWDIR}"
echo "LOGDIR                     : ${log_dir}"
echo "SLURM_NODEFILE             : ${SLURM_NODEFILE}"
echo "physcpubind                : ${physcpubind}"
echo "facility_address_book      : ${facility_address_book}"
echo "net_type                   : ${net_type}"

mkdir -p "${log_dir}" #only on head node
SLURM_NODEFILE=${log_dir}/node_file.txt
scontrol show hostnames "$SLURM_JOB_NODELIST" > "$SLURM_NODEFILE"
clush --hostfile "${SLURM_NODEFILE}" "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1
clush --hostfile "${SLURM_NODEFILE}" "rm -rf ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1
clush --hostfile "${SLURM_NODEFILE}" "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1 # on all compute nodes
# clush --hostfile "${SLURM_NODEFILE}" "ls -lah  ${CU_FUSE_MNT_VIEWDIR}/" 
# sleep "${sleeptime}"s # add 60s if you are running on more than 2k nodes

echo "SLURM_JOB_NUM_NODES" ${SLURM_JOB_NUM_NODES}
options=" -f -tpath / -vpath "${CU_FUSE_MNT_VIEWDIR}" -log_level $log_level -log_type $log_type -log_output_dir $log_dir -net_type ${net_type} -trees $trees -nf $SLURM_NODEFILE -max_cacheable_byte_size $max_cacheable_byte_size  -facility_address_book ${facility_address_book} -s $CU_FUSE_MNT_VIEWDIR"

srun --overlap -N ${SLURM_NNODES} --ntasks-per-node=1 --cpus-per-task=2 --cpu-bind=verbose,none \
  bash -c '
    echo "host=$(hostname)"
    taskset -c '"${physcpubind}"' bash -c '"'"'
      echo "allowed=$(grep Cpus_allowed_list /proc/self/status | awk "{print \$2}")"
      echo "current_cpu=$(ps -o psr= -p $$)"
      exec '"${CUPATH}"' '"${options}"'
    '"'"'
  ' & 



echo "sleeping for ${sleeptime} s"
sleep "${sleeptime}"s # add 60s if you are running on more than 2k nodes

echo "Launching Copper Gracefully On All Nodes : End" 
