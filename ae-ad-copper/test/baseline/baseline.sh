#!/bin/bash -x
#
# Baseline to load torch where all processes are loading
#


module load frameworks

nodes=$(cat $PBS_NODEFILE | wc -l)

echo "Launch Baseline Nodes:$nodes" 1>&2
time mpiexec -n $((nodes*12)) -ppn 12 sleep 1

echo "Baseline First Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
time mpiexec -n $((nodes*12)) -ppn 12 \
    --genvall \
    --genv=PYTHONPATH=/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env \
    --cpu-bind=list:4-51 \
    python3 -c "import torch; print(torch.__file__)"

echo "Baseline Cached Nodes:$nodes Python:12 ES:1 fcores:1 total:2" 1>&2
time mpiexec -n $((nodes*12)) -ppn 12 \
    --genvall \
    --genv=PYTHONPATH=/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env \
    --cpu-bind=list:4-51 \
    python3 -c "import torch; print(torch.__file__)"

