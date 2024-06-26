#!/bin/bash
set -o nounset

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

cd build || { echo "Failed to move to build dir"; exit 1; }
make || { echo "Failed to build mongo_fuse_fs"; exit 1; }

