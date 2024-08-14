#!/bin/bash -x
#
# Single Node "GPU" Test case
# 12 python ranks distributed on second socket
#   using dedicated set of cores
# Copper running on a dedicated set of cores
# FUSE always runs with 10 threads

module load frameworks

nodes=$(cat $PBS_NODEFILE | wc -l)

echo "Dataloader-Copper Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
time mpiexec -n $((nodes*12)) -ppn 12 \
    --cpu-bind=list:4-51 \
    python3 main.py workload=unet3d_copper ++workload.workflow.generate_data=False ++workload.workflow.train=True ++workload.workflow.checkpoint=False

