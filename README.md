# Copper - Co-operative Caching Layer for scalable parallel data movement in Exascale Supercomputers

### Introduction

Copper is a read-only cooperative caching layer aimed to enable scalable data loading on massive amounts of compute nodes. This aims to avoid the I/O bottleneck in the storage network and effectively use the compute network for data movement.

The current intended use of copper is to improve the performance of python imports - dynamic shared library loading on Aurora. However, copper can used to improve the performance of any type of redundant data loading on a supercomputer.

More documentation can be found here: [readthedocs](https://alcf-copper-docs.readthedocs.io/en/latest/)

![gifmaker_me](https://github.com/user-attachments/assets/9a6a12a0-2003-42a0-80e2-330305c38524)

### Lists of the current filesystem operations that are supported.

- [X] init
- [X] open
- [X] read
- [X] readdir
- [X] readlink
- [X] getattr
- [X] ioctl
- [X] destroy

### How to load the copper package on Aurora

```bash
module load copper
```

### How to start the copper service

```bash
launch_copper.sh 
```

### How to run your app with copper

```bash
# The below line is required only for the first time setup to install a package on a custom directory. 
# python -m pip install  --target=/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/ dragonhpc

time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
            --genv=PYTHONPATH=/tmp/${USER}/copper/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/:$PYTHONPATH \
             python3 -c "import dragon; print(dragon.__file__)"
```

### How to stop the copper service

```bash
stop_copper.sh # optional - enabled by default on the PBS epilog during cleanup.
```

### Best practices

1. Prepending `/tmp/${USER}/copper`/ to different environment variables should be done in a careful and need basis manner.
2. You should not prepend copper path blindly to all known variables like `PYTHON_PATH, VIRTUAL_ENV, CONDA_PREFIX, CONDA_ROOT, LD_LIBRARY_PATH and PATH`.
3. When using python virutal environment, prepending `/tmp/${USER}/copper/` only to `PYTHONPATH` variable is sufficient.
4. When using a personal conda environment, prepending `/tmp/${USER}/copper/` only to `PYTHONPATH` variable is sufficient.
5. If you prefer the python and pip binary or any other binary under the virtual_env or conda_prefix to be in copper path for some reason, only then you should prepend `/tmp/${USER}/copper/..path_to_venv or path_to_conda ..bin/`  to $`PATH` variable.
6. If your application is taking input files as argument, which you prefer to go through copper, you can prepend `/tmp/${USER}/copper/` to the input file path argument only.
7. If there is a specific library file that you want to go through copper, you can prepend `/tmp/${USER}/copper/` to the `LD_LIBRARY_PATH` variable. Again prepending copper path to all paths in `LD_LIBRARY_PATH` is not recommended.
8. Copper is read only and cannot be used to write any files. So, you should not use copper path for any output files or temporary files.
9. Copper runs by default on cores `physcpubind="48-51"` which should not be used in your application cpu bind list. You can also change the copper cores by `launch_copper.sh -b "core_range"` .
10. You should be aware and cautious of any other hardcoded paths in your package or your application.
11. The current copper supported filesystem operations are  init, open, read, readdir, readlink, getattr, ioctl, destroy.
12. Note, write, unlink, rename, mkdir, rmdir, symlink, statfs, fsync, flush, mmap and other operations are not supported.
13. System default modules like frameworks, python, intel, mpich whose metadata are baked into the os image do not require copper.

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

spack add mochi-thallium
spack install

# incase of any issue with the spack environment, completely delete spack and start again
spack env remove kaushik_env_1 

# from the next time onwards you only need 
.  copper/gitrepos/git-spack-repo/spack/share/spack/setup-env.sh 
spack env activate kaushik_env_1 
```

### How to locally build copper

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

### Future Works

1. Increase the fuse max_read FS kernel value to improve the fuse read performance
2. Provide to flag to choose RDMA instead of the current RPC only mode.
3. Support for multinics. currently only single nic per node is supported
4. Note filesystem operations like write, unlink, rename, mkdir, rmdir, symlink, statfs, fsync, flush, mmap and other operations are not supported.
