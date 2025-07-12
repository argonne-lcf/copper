#!/bin/bash -x
#PBS -l select=512
#PBS -l walltime=02:00:00
#PBS -A datascience
#PBS -q prod
#PBS -k doe

# qsub -l select=512:ncpus=208 -l walltime=01:00:00 -A datascience -l filesystems=home:flare -q prod  ./simple_with_copper.sh # or - I 

# This example shows loading python modules from a lustre directory with using copper.
 
cd $PBS_O_WORKDIR
echo Jobid: $PBS_JOBID
echo Running on nodes `cat $PBS_NODEFILE`

NNODES=`wc -l < $PBS_NODEFILE`
RANKS_PER_NODE=12
NRANKS=$(( NNODES * RANKS_PER_NODE ))
echo "App running on NUM_OF_NODES=${NNODES}  TOTAL_NUM_RANKS=${NRANKS}  RANKS_PER_NODE=${RANKS_PER_NODE}"

module load frameworks
module load copper 
launch_copper.sh -s 5 # you should update the sleep time based on your job size. 5 seconds is a good default for 2 nodes 20s for 2k nodes is preferred

# The below line is required only for the first time setup to install a package on a custom directory. 
# python -m pip install  --target=/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/ dragonhpc


# Best practices for using copper. 

# 1. Prepending /tmp/${USER}/copper/ to different environment variables should be done in a careful and need basis manner. 
# 2. You should not prepend copper path blindly to all known variables like PYTHON_PATH, VIRTUAL_ENV, CONDA_PREFIX, CONDA_ROOT, LD_LIBRARY_PATH and PATH.
# 3. For this example, prepending to only PYTHONPATH is sufficient. 
# 4. When using python virutal environment, prepending /tmp/${USER}/copper/ only to PYTHONPATH variable is sufficient. 
# 5. When using a personal conda environment, prepending /tmp/${USER}/copper/ only to PYTHONPATH variable is sufficient.  
# 6. If you prefer the python and pip binary or any other binary under the virtual_env or conda_prefix to be in copper path for some reason, only then you should prepend /tmp/${USER}/copper/..path_to_venv or path_to_conda ..bin/ to $PATH variable.
# 7. If your application is taking input files as argument, which you prefer to go through copper, you can prepend /tmp/${USER}/copper/ to the input file path argument only.
# 8. If there is a specific library file that you want to go through copper, you can prepend /tmp/${USER}/copper/ to the LD_LIBRARY_PATH variable. Again prepending copper path to all paths in LD_LIBRARY_PATH is not recommended.
# 9. Copper is read only and cannot be used to write any files. So, you should not use copper path for any output files or temporary files. 
# 10. Copper runs by default on cores physcpubind="48-51" which should not be used in your application cpu bind list. You can also change the copper cores by launch_copper.sh -b "core_range" .  
# 11. You should be aware and cautious of any other hardcoded paths in your package or your application.          
# 12. The current copper supported filesystem operations are  init, open, read, readdir, readlink, getattr, ioctl, destroy. 
# 13. Note, write, unlink, rename, mkdir, rmdir, symlink, statfs, fsync, flush, mmap and other operations are not supported.
# 14. System default modules like frameworks, python, intel, mpich whose metadata are baked into the os image do not require copper.


time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
            --genv=PYTHONPATH=/tmp/${USER}/copper/lus/flare/projects/datascience/kaushik/copper-test/lus_custom_pip_env/:$PYTHONPATH \
             python3 -c "import dragon; print(dragon.__file__)"

stop_copper.sh # optional - enabled by default on the PBS epilog during cleanup.
