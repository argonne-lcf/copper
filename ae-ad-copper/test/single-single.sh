#!/bin/bash
#
# Single Node "GPU" Test case
# 12 python ranks distributed on second socket
#   using dedicated set of cores
# Copper running on a dedicated set of cores
# FUSE always runs with 10 threads


CUPATH=/home/harms/working/copper/copper/build

mkdir -p /tmp/copper
mkdir -p /tmp/copper-logs

    numactl --physcpubind="0,4" \
      $CUPATH/cu_fuse \
      -tpath / \
      -vpath /tmp/copper \
      -log_level 6 \
      -log_type file \
      -log_output_dir /tmp/copper-logs \
      -es 1 -s -f \
      /tmp/copper &

    sleep 60
    pid=$(ps h -C cu_fuse | awk '{print $1}')

    mpiexec -n 12 \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy; print(numpy.__file__)"

    echo "Cached Python:12 ES:1 fcores:1 total:2" 1>&2
    time mpiexec -n 12 \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy; print(numpy.__file__)"

    fusermount3 -u /tmp/copper
    sleep 2
    kill ${pid}
