#!/bin/bash
set -o nounset

# module use /soft/preview-modulefiles/24.086.0
# module load frameworks/2024.04.15.002
# module list
# . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
# spack env activate kaushik_env_1 
# cd $PBS_O_WORKDIR
# echo Jobid: $PBS_JOBID
# echo Running on nodes `cat $PBS_NODEFILE`
# NNODES=`wc -l < $PBS_NODEFILE`
# RANKS_PER_NODE=1
# NRANKS=$(( NNODES * RANKS_PER_NODE ))
# echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
# EXT_ENV="--env FI_CXI_DEFAULT_CQ_SIZE=1048576 " 
# which python
# source /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/scripts/env.sh


if [ ! -f ../env.sh ]
then
	echo "failed to find ../scripts/env.sh"
	exit 1
fi

source ../env.sh

cd "$ROOT_DIR"

if [ -d build ]
then
	echo "found build dir"
else
	echo "creating build dir"
	mkdir build
fi

cmake -DCMAKE_PREFIX_PATH=/lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb  -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Bbuild || { echo "Failed to create compile commands"; exit 1; }
cp build/compile_commands.json . || { echo "Failed to copy compile commands"; exit 1; }


cd build || { echo "Failed to move to build dir"; exit 1; }
make || { echo "Failed to build mongo_fuse_fs"; exit 1; }

