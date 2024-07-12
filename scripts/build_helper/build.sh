#!/bin/bash

# module use /soft/preview-modulefiles/24.086.0
# module load frameworks/2024.04.15.002
# module list
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

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh

if [ ! -f $ENV ]
then
	echo "failed to find ENV: $ENV"
	exit 1
fi

source $ENV

cd "$ROOT_DIR"

if [ -d build ]
then
	echo "found build dir"
else
	echo "creating build dir"
	mkdir build
fi

cmake -DBLOCK_REDUNDANT_RPCS="$BLOCK_REDUNDANT_RPCS" \
      -DCMAKE_VERBOSE_MAKEFILE=ON        \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DFUSE3_LIB=$FUSE3_LIB             \
      -DFUSE3_INCLUDE=$FUSE3_INCLUDE     \
      -Bbuild || { echo "Failed to create compile commands"; exit 1; }
cp build/compile_commands.json . || { echo "Failed to copy compile commands"; exit 1; }


cd build || { echo "Failed to move to build dir"; exit 1; }
make || { echo "Failed to build mongo_fuse_fs"; exit 1; }

