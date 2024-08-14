#!/bin/bash -x
#
# Single Node "GPU" Test case
# 12 python ranks distributed on second socket
#   using dedicated set of cores
# Copper running on a dedicated set of cores
# FUSE always runs with 10 threads

CUPATH=/home/harms/working/copper/copper/build
LOGDIR=/home/harms/copper-logs/${PBS_JOBID%%.aurora-pbs-0001.hostmgmt.cm.aurora.alcf.anl.gov}

mkdir -p ${LOGDIR}
#rm -f ${LOGDIR}/copper_address_book.txt
#touch ${LOGDIR}/copper_address_book.txt

#
# clean old mounts
#
WCOLL=$PBS_NODEFILE pdsh -t 60 fusermount3 -u /tmp/copper

#
# create mount point
#
WCOLL=$PBS_NODEFILE pdsh mkdir -p /tmp/copper

module load frameworks

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

    nodes=$(cat $PBS_NODEFILE | wc -l)

    sleep 60

    echo "Launch Baseline Nodes:$nodes" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 sleep 1

    echo "First Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 \
	    --genvall \
	    --genv=PYTHONPATH=/tmp/copper/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env \
	    --cpu-bind=list:4-51 \
	    python3 -c "import torch; print(torch.__file__)"

    echo "Cached Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 \
	    --genvall \
	    --genv=PYTHONPATH=/tmp/copper/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env \
	    --cpu-bind=list:4-51 \
	    python3 -c "import torch; print(torch.__file__)"

    WCOLL=$PBS_NODEFILE pdsh -t 60 fusermount3 -u /tmp/copper
