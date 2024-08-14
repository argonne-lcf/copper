#!/bin/bash -x

module load frameworks

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
  python3 -c "import mpi4py; print(mpi4py.__file__)"
