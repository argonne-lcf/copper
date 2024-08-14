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

WCOLL=$PBS_NODEFILE pdsh mkdir -p /tmp/copper

  #gdb-oneapi -ex="r -tpath / -vpath /tmp/copper -log_level 1 -log_type file -log_output_dir ${LOGDIR} -es 1 -nf ${PBS_NODEFILE} -s /tmp/copper"
read -r -d '' CMD << EOM
   $CUPATH/cu_fuse
     -tpath /
     -vpath /tmp/copper
     -log_level 5
     -log_type file
     -log_output_dir ${LOGDIR}
     -es 1
     -nf ${PBS_NODEFILE}
     -s
     /tmp/copper
EOM

    WCOLL=$PBS_NODEFILE pdsh -t 60 $CMD

    nodes=$(cat $PBS_NODEFILE | wc -l)

    sleep 60

    echo "Launch Baseline Nodes:$nodes" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c ""

    echo "First Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 \
	    --genvall \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy; print(numpy.__file__)"

    echo "Cached Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
    time mpiexec -n $((nodes*12)) -ppn 12 \
	    --genvall \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy; print(numpy.__file__)"

    WCOLL=$PBS_NODEFILE pdsh -t 60 fusermount3 -u /tmp/copper
