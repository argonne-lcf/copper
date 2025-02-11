#!/bin/bash -x

RANKS_PER_NODE=1
echo "App running on NUM_OF_NODES=${SLURM_JOB_NUM_NODES}  RANKS_PER_NODE=${RANKS_PER_NODE} "

echo "Launching Copper Gracefully On All Nodes : Start" 
COPPER_ROOT=/lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/build
log_level=1
log_type="file"
trees=1
max_cacheable_byte_size=$((10*1024*1024))
sleeptime=20
LOGDIR=~/copper-logs/${SLURM_JOB_ID}
rm -rf ~/copper_logs*
CUPATH=$COPPER_ROOT/cu_fuse
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
physcpubind="51-55"
facility_address_book="/lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/olcf_copper_addressbook.txt";
SLURM_NODEFILE=/lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/examples/example3/frontier_nodes.txt
rm $SLURM_NODEFILE
scontrol show hostnames > $SLURM_NODEFILE

while getopts "l:t:T:M:s:b:" opt; do
  case ${opt} in
    l ) log_level=$OPTARG ;;
    t ) log_type=$OPTARG ;;
    T ) trees=$OPTARG ;;
    M ) max_cacheable_byte_size=$OPTARG ;;
    s ) sleeptime=$OPTARG ;;
    b ) physcpubind=$OPTARG ;;
    \? ) echo "Usage: cmd [-l] [-t] [-T] [-M] [-s] [-b]" ;;
  esac
done

echo "log_level                  : ${log_level}"
echo "log_type                   : ${log_type}"
echo "trees                      : ${trees}"
echo "max_cacheable_byte_size    : ${max_cacheable_byte_size}"
echo "sleeptime                  : ${sleeptime}"
echo "CU_FUSE_MNT_VIEWDIR        : ${CU_FUSE_MNT_VIEWDIR}"
echo "LOGDIR                     : ${LOGDIR}"
echo "SLURM_NODEFILE             : ${SLURM_NODEFILE}"
echo "physcpubind                : ${physcpubind}"



mkdir -p "${LOGDIR}" #only on head node
clush --hostfile "${SLURM_NODEFILE}" "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${SLURM_NODEFILE}" "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile "${SLURM_NODEFILE}" "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" # on all compute nodes

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
     -nf ${SLURM_NODEFILE}
     -facility_address_book ${facility_address_book}
     -max_cacheable_byte_size ${max_cacheable_byte_size}
     -s ${CU_FUSE_MNT_VIEWDIR}
EOM

# clush --hostfile "${PBS_NODEFILE}" $CMD
options="-tpath \ -vpath "${CU_FUSE_MNT_VIEWDIR}" -log_level $log_level -log_type $log_type -log_output_dir $LOGDIR -net_type cxi -trees $trees -nf $SLURM_NODEFILE -max_cacheable_byte_size $max_cacheable_byte_size -s $CU_FUSE_MNT_VIEWDIR"
srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE --network=single_node_vni,job_vni $CUPATH $options 

sleep "${sleeptime}"s # add 60s if you are running on more than 2k nodes

echo "Launching Copper Gracefully On All Nodes : End" 

# export SLINGSHOT_SVC_IDS=5,5,5,5
# export SLINGSHOT_VNIS=1358,1214
# export SLINGSHOT_DEVICES=cxi0,cxi1,cxi2,cxi3
# export SLINGSHOT_TCS=0xa

# clush --hostfile "${SLURM_NODEFILE}" "env SLINGSHOT_SVC_IDS="5,5,5,5" SLINGSHOT_VNIS="2789,2130" SLINGSHOT_DEVICES="cxi0,cxi1,cxi2,cxi3" SLINGSHOT_TCS="0xa" " $CMD
# srun -N $SLURM_JOB_NUM_NODES --ntasks-per-node=$RANKS_PER_NODE clush --hostfile "${SLURM_NODEFILE}"  $CMD

#  srun -N 1 --ntasks-per-node=1 -c 1 --network=single_node_vni,job_vni  hostname
# --threads-per-core=2 
# --cpu-bind=threads
# -c, --cpus-per-task=<ncpus>
# --cpu-bind=map_cpu:0,1,2,3
#  -u --exclusive

#  clush --hostfile="/lustre/orion/gen008/proj-shared/kaushik/gitrepos/copper/examples/example3/frontier_nodes.txt"  "env LD_LIBRARY_PATH=/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mochi-margo-0.18.3-broq2iugtv4ox2gmnhndyiqgpa5gg7cg/lib/:/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/mercury-2.4.0rc5-iwwrnwcrfhruw2q7ljggo57zcicwzesi/lib:/lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/opt/spack/linux-sles15-zen3/gcc-12.3.0/argobots-1.2-xibbetpmkr6xq7iyrl6ay4pql74mwshl/lib/:$LD_LIBRARY_PATH SLINGSHOT_SVC_IDS="5,5,5,5" SLINGSHOT_VNIS="1358,1214" SLINGSHOT_DEVICES="cxi0,cxi1,cxi2,cxi3" SLINGSHOT_TCS="0xa" " /lustre/orion/gen008/proj-shared/kaushik/gitrepos/margo-tests/simple-margo 