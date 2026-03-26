#!/bin/bash

#spack env activate spack-copper-mod-env

SCRIPT=$(realpath -s "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
ENV=$SCRIPTPATH/../env.sh
export CC=${CC:-$(which gcc)}
export CXX=${CXX:-$(which g++)}

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

cmake -DCMAKE_BUILD_TYPE=Release                     \
	  -DBLOCK_REDUNDANT_RPCS="$BLOCK_REDUNDANT_RPCS" \
      -DCMAKE_VERBOSE_MAKEFILE=ON                    \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON             \
      -DFUSE3_LIB=${FUSE3_LIB_FILE}                         \
      -DFUSE3_INCLUDE=${FUSE3_INCLUDE_DIR}                 \
      -Bbuild || { echo "Failed to create compile commands"; exit 1; }

cp build/compile_commands.json . || { echo "Failed to copy compile commands"; exit 1; }


cd build || { echo "Failed to move to build dir"; exit 1; }
make || { echo "Failed to build cu_fuse"; exit 1; }
make install 

