# Copper - Co-operative Caching Layer for scalable parallel data movement 

### Introduction 

Built with fuse and mochi thallium RPC library... 
Image of the tree.


### Lists of the current filesystem operations that are supported.

- [x] destroy
- [x] init
- [x] ioctl
- [x] readdir
- [x] read
- [x] open
- [x] getattr
- [x] readlink


### Setting up mochi spack environment on Aurora

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

# incase of any issue with the spack environment, completely delete spack with 
spack env remove kaushik_env_1 
and start again

# from the next time onwards you only need 
.  copper/gitrepos/git-spack-repo/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
```

### How to build Copper
 
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

### How to start the copper service

```
mpirun -np 7 -ppn 1 copper/scripts/filesystem/mnt_fs.sh [& or separate terminal ]
```

### How to start the real app

```
module load frameworks 

#setup PY_PACKAGES_DIR
pip install --target=/lus/flare/projects/Aurora_deployment/kaushik/copper/july12/copper/run/copper_conda_env torch 

cd copper/july7/copper/scripts
mpirun -np 7 -ppn 1 copper/scripts/tests/run_test.sh [update this file and run in a separate terminal]

```

### To stop the copper service

```
# mpirun -np 7 -ppn 1 fusermount3 -u /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/july1/copper/run/cu_fuse_view
# mpirun -np 1 -ppn 1 build/rpc_shutdown #needs to be run only on 1 node with 1 ppn and requires the copper address book
```
