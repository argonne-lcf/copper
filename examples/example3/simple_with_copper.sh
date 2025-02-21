#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A Aurora_deployment
#PBS -q lustre_scaling
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=01:00:00 -A Aurora_deployment -l filesystems=home:flare -q prod  ./simple_with_copper.sh # or - I 

# This example shows loading python modules from a lustre directory with using copper.
 
cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

module load copper 
launch_copper.sh
# Prepend /tmp/${USER}/copper/ to all your absolute paths if you want your I/O to go through copper (including PYTHON_PATH, CONDA_PREFIX, CONDA_ROOT and PATH)

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

# The below 2 lines are only for the first time setup to install a package on a custom dir. Do not use in this job script
# module load frameworks
# python -m pip install  --target=/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/ torch==2.3.1+cxx11.abi torchvision==0.18.1+cxx11.abi torchaudio==2.3.1+cxx11.abi intel-extension-for-pytorch==2.3.110+xpu oneccl_bind_pt==2.3.100+xpu --extra-index-url https://pytorch-extension.intel.com/release-whl/stable/xpu/us/
# module unload frameworks

module load python
export LD_LIBRARY_PATH=${CONDA_DIR}:/opt/aurora/24.180.3/oneapi/2024.2/lib/:/opt/aurora/24.180.3/oneapi/compiler/2024.2/lib/:/opt/aurora/24.180.3/oneapi/intel-conda-miniforge/envs/2024.2.0/lib/:/opt/aurora/24.180.3/oneapi/intel-conda-miniforge/pkgs/intel-sycl-rt-2024.2.1-intel_1079/lib/:/opt/aurora/24.180.3/updates/oneapi/compiler/eng-20240629/lib/:/opt/aurora/24.180.3/support/tools/pti-gpu/d3639de/lib64/:$LD_LIBRARY_PATH

time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:9:14:19:20:25:56:61:66:71:74:79 --genvall \
            --genv=PYTHONPATH=/tmp/${USER}/copper/lus/flare/projects/Aurora_deployment/kaushik/copper/copper-test-apps/torch-with-copper/lus_pip_torch_2.3_env_1/ \
            python3 -c "import torch; print(torch.__file__)"

stop_copper.sh # optional 
