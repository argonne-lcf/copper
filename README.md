# Copper - Co-operative Caching Layer for scalable parallel data movement in Exascale Supercomputers

### Introduction 

Copper is a read-only cooperative caching layer aimed to enable scalable data loading on massive amounts of compute nodes. This aims to avoid the I/O bottleneck in the storage network and effectively use the compute network for data movement. 

The current intended use of copper is to improve the performance of python imports - dynamic shared library loading on Aurora. However, copper can used to improve the performance of any type of redundant data loading on a supercomputer. 

More documentation can be found here: [readthedocs](https://alcf-copper-docs.readthedocs.io/en/latest/)

![gifmaker_me](https://github.com/user-attachments/assets/9a6a12a0-2003-42a0-80e2-330305c38524)



### Lists of the current filesystem operations that are supported.

- [x] init
- [x] open
- [x] read
- [x] readdir
- [x] readlink
- [x] getattr
- [x] ioctl
- [x] destroy



### How to load the copper package on Aurora

```bash
module load copper
```


### How to start the copper service

```bash
CUPATH=$COPPER_ROOT/bin/cu_fuse # If you are building copper on your own, set this path to your cu_fuse binary
LOGDIR=~/copper-logs/${PBS_JOBID}
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
mkdir -p ${LOGDIR}
clush --hostfile ${PBS_NODEFILE} "mkdir -p ${CU_FUSE_MNT_VIEWDIR}"

read -r -d '' CMD << EOM
   numactl --physcpubind="0-3"
   $CUPATH/cu_fuse 
     -tpath /                                   # / will be mounted under CU_FUSE_MNT_VIEWDIR
     -vpath ${CU_FUSE_MNT_VIEWDIR}              # To provide the fuse mounted location
     -log_output_dir ${LOGDIR}                  # To provide where the copper logs will be stored
     -log_level 6                               # To provide the level of copper logging 6 more 0 less
     -log_type file                             # To direct logging to file / stdout / both
     -net_type cxi                              # To provide the network protocol
     -nf ${PBS_NODEFILE}                        # To provide the hostlist where cu_fuse will be mounted
     -trees 1                                   # To provide the number of trees to form in the overlay network
     -max_cacheable_byte_size $((10*1024*1024)) # To provide the size of access that goes through copper
     -facility_address_book ${facility_address_book} # To provide the path to the facility_address_book file
     -s ${CU_FUSE_MNT_VIEWDIR}                  # To start fuse in single threaded mode.
EOM

clush --hostfile ${PBS_NODEFILE} $CMD           # To start copper on all the compute nodes

# instead of clush you can also use the following to start copper as a background process on all compute nodes 
# WCOLL=$PBS_NODEFILE pdsh $CMD
# mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:0-3 sh ./scripts/filesystem/mnt_fs.sh & 
```

### How to run your app with copper

```bash

time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=${CPU_BINDING} --genvall \
            --genv=PYTHONPATH=${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env \
            python3 -c "import torch; print(torch.__file__)"
```

### How to stop the copper service

```bash
clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "pkill  -9 cu_fuse"
```


### How to set up a personal mochi spack copper environment on Aurora

In order to build we recommend installing mochi dependencies using spack and their environment feature. You can find the instructions to install spack [here](https://spack-tutorial.readthedocs.io/en/latest/tutorial_basics.html). The required mochi services are margo, mercury, thallium, and cereal. The instructions to install the listed mochi services can be found [here](https://mochi.readthedocs.io/en/latest/installing.html).

Assuming you have a mochi environment setup correctly you should now be able to build by running the following commands.

```bash
git clone https://github.com/spack/spack.git
.  copper/gitrepos/git-spack-repo/spack/share/spack/setup-env.sh 

git clone https://github.com/mochi-hpc-experiments/platform-configurations.git
cd  copper/gitrepos/git-mochi-repos/platform-configurations/ANL/Aurora 
[compare with copper/scripts/build_helper/aurora_spack.yaml]

git clone https://github.com/mochi-hpc/mochi-spack-packages.git
spack repo add copper/gitrepos/git-mochi-repos/mochi-spack-packages

module load cmake # on aurora
spack env create kaushik_env_1 spack.yaml 
spack env activate kaushik_env_1 

spack add mochi-margo
spack install

# incase of any issue with the spack environment, completely delete spack and start again
spack env remove kaushik_env_1 

# from the next time onwards you only need 
.  copper/gitrepos/git-spack-repo/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
```

### How to build copper
 
```bash
git clone https://github.com/argonne-lcf/copper  
cd copper/scripts/build_helper/
cp default_env.sh env.sh

Set the following variables in the copied `env.sh`
export VIEW_DIR=<PATH_TO_MOUNT_DIRECTORY>
export FUSE3_LIB=<PATH_TO_FUSE_LIBRARY>
export FUSE3_INCLUDE=<PATH_TO_FUSE_HEADER>
export PY_PACKAGES_DIR=<PATH_TO_PY_ENV>

spack env activate <MOCHI_ENV>
sh copper/scripts/build_helper/build.sh
```
