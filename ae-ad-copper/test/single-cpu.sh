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

corelist=("" \
	  "0" \
	  "0,1" \
	  "0,1,2" \
	  "0,1,2,3" \
	  "0,1,2,3,4" \
	  "0,1,2,3,4,5" \
	  "0,1,2,3,4,5,6" \
	  "0,1,2,3,4,5,6,7" \
	  "0,1,2,3,4,5,6,7,8" \
	  "0,1,2,3,4,5,6,7,8,9" \
	  "0,1,2,3,4,5,6,7,8,9,10" \
	  "0,1,2,3,4,5,6,7,8,9,10,11")

for es in {1,2,4};
do
  for fcores in {1,2,4,8};
  do
    i=$((es+fcores))
    list=${corelist[$i]}
    numactl --physcpubind="${list}" \
      $CUPATH/cu_fuse \
      -tpath / \
      -vpath /tmp/copper \
      -log_level 6 \
      -log_type file \
      -log_output_dir /tmp/copper-logs \
      -es ${es} -f \
      /tmp/copper &

    sleep 60
    pid=$(ps h -C cu_fuse | awk '{print $1}')

    mpiexec -n 52 \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy"

    echo "Cached Python:12 ES:${es} fcores:${fcores} total:${i}" 1>&2
    time mpiexec -n 52 \
	    --genv=PYTHONPATH=/tmp/copper/soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/lib/python3.9/site-packages \
	    --cpu-bind=list:52-103 \
	    /soft/compilers/oneapi/2023.12.15.001/oneapi/intelpython/python3.9/bin/python3 -c "import numpy; print(numpy.__file__)"

    fusermount3 -u /tmp/copper
    sleep 2
    kill ${pid}
    sleep 10

  done
done
