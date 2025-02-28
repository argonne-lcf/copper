#!/bin/bash
. /lustre/orion/gen008/proj-shared/kaushik/gitrepos/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
spack load mochi-margo
spack load mochi-thallium
rm -rf ../../build/*

mkdir build

cmake  -DCMAKE_BUILD_TYPE=Release  -DCMAKE_VERBOSE_MAKEFILE=ON  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON  -Bbuild || { echo "Failed to create compile commands"; exit 1; }

cd build || { echo "Failed to move to build dir"; exit 1; }
make || { echo "Failed to build cu_fuse"; exit 1; }
make install 

