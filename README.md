# Copper - Co-operative Caching Layer for scalable parallel data movement in Exascale Supercomputers

### Introduction 

Copper is a read-only cooperative caching layer aimed to enable scalable data loading on massive amounts of compute nodes. This aims to avoid the I/O bottleneck in the storage network and effectively use the compute network for data movement. 

More documentation can be found here: [readthedocs](https://alcf-copper-docs.readthedocs.io/en/latest/)

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

```
module load spack-pe-oneapi copper
```


### How to start the copper service

```

CUPATH=/lus/flare/projects/Aurora_deployment/kaushik/copper-spack-recipe/gitrepos/copper/build
LOGDIR=~/copper-logs/${PBS_JOBID}
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
rm -rf ~/copper_logs*
mkdir -p ${LOGDIR}
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "mkdir -p ${CU_FUSE_MNT_VIEWDIR}"

read -r -d '' CMD << EOM
   numactl --physcpubind="0-3"
   $CUPATH/cu_fuse 
     -tpath /                                   # / will be mounted under CU_FUSE_MNT_VIEWDIR
     -vpath ${CU_FUSE_MNT_VIEWDIR}              # To provide the fuse mounted location
     -log_output_dir ${LOGDIR}                  # To provide where the copper logs will be stored
     -log_level 6                               # To provide the level of copper logging 
     -log_type file                             # To direct logging to file / stdout / both
     -net_type cxi                              # To provide the network protocol
     -nf ${PBS_NODEFILE}                        # To provide the hostlist where cu_fuse will be mounted
     -trees 1                                   # To provide the number of trees to form in the overlay network
     -max_cacheable_byte_size $((10*1024*1024)) # To provide the size of access that goes through copper
     -s ${CU_FUSE_MNT_VIEWDIR}                  # To start fuse in single threaded mode.
EOM

clush --hostfile ${PBS_NODEFILE} $CMD           # To start copper on all the compute nodes

# instead of clush you can also use the following to start copper as a background process on all compute nodes 
# WCOLL=$PBS_NODEFILE pdsh $CMD
# mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:0-3 sh ./scripts/filesystem/mnt_fs.sh & 

```

### How to run your app with copper

```

RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"
module use /lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024
module load frameworks/2024.1
conda deactivate
conda activate ${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024 #Start conda with the full copper path instead of the standard path
which python
CPU_BINDING=list:4:9:14:19:20:25:56:61:66:71:74:79 
time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=${CPU_BINDING} --genvall --genv=PYTHONPATH=${CU_FUSE_MNT_VIEWDIR}/lus/flare/projects/Aurora_deployment/copper-software-module/example_app/app-dependencies/sst_2024  python3 real_app.py

```

### How to stop the copper service

```

clush --hostfile ${PBS_NODEFILE} "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}"
clush --hostfile ${PBS_NODEFILE} "rm -rf ${CU_FUSE_MNT_VIEWDIR}"
export UID=$(id -u $USER)
clush --hostfile ${PBS_NODEFILE} "pkill -U $UID"

```


### How to set up a personal mochi spack copper environment on Aurora

In order to build we recommend installing mochi dependencies using spack and their environment feature. You can find the instructions to install spack [here](https://spack-tutorial.readthedocs.io/en/latest/tutorial_basics.html). The required mochi services are margo, mercury, thallium, and cereal. The instructions to install the listed mochi services can be found [here](https://mochi.readthedocs.io/en/latest/installing.html).

Assuming you have a mochi environment setup correctly you should now be able to build by running the following commands.

```
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
 
```
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
