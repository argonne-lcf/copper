#!/bin/bash -x
# qsub -l nodes=1 -q workq  -l walltime=00:20:00 -l filesystems=gila -A  Aurora_deployment -I 
# qsub -l nodes=8 -q workq  -l walltime=00:05:00 -l filesystems=gila -A  Aurora_deployment -I 

#PBS -A Aurora_deployment
#PBS -k doe

module list
module use /soft/modulefiles
module load frameworks/2023.12.15.001  

cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`
# echo $PBS_NODEFILE
# cat $PBS_NODEFILE

# Preparing the spack stuff and its environment
. /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
module load frameworks/2023.12.15.001  
which python
which spack
spack find 
spack env activate kaushik_env_1 
spack find 
spack load mochi-margo argobots   mochi-abt-io     mochi-ssg  libfabric   mochi-bedrock  mochi-thallium   py-mpi4py   mercury        mochi-margo 

# To compile - Adding this for easy understanding 
mpicxx -std=c++14 \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-thallium-0.12.0-e5qylejlhcuvbiez6j76o2kp2y6jf4ul/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/include/ \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/include/  \
    -I /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-x86_64/gcc-12.2.0/cereal-1.3.2-5rhyn3lhfeug6gpf5wsssoz5bvilgpzm/include/  \
    -o copper copper.cpp \
    -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mochi-margo-0.15.0-tczxmksittter2koo6kr24zdmkp3zjl5/lib/  -lmargo \
    -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/mercury-2.3.1-3z7ogv2czvxdvwjf4onqdurc5kizw4gq/lib/  -lmercury  \
    -L /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/opt/spack/linux-sles15-icelake/gcc-12.2.0/argobots-1.2-kdwec3ak5572uir2nes34bqq5fcpyisb/lib/  -labt 

# To run the copper process 1 process per node 

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=1          # Number of MPI ranks per node
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

# Add the echo $PBS_NODEFILE as the argument 

mpiexec -np ${NRANKS} -ppn ${RANKS_PER_NODE} ./copper 
