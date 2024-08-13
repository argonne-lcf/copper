Building and Running
====================

Installing
^^^^^^^^^^^^^^^^^^^^^^

Copper uses several mochi depencies to run. The instructions below
will use Spack build tool which can be found [here](https://spack-tutorial.readthedocs.io/en/latest/index.html)
to build and install the required mochi dependencies. The rest of
the installation assumes Spack is properly installed and that
``spack`` command is available on the shell.

Create a Spack environment where the dependencies will be installed.

::

      spack env create copper_mochi
      spack activate copper_mochi

Clone the mochi Spack packages repository and add to Spack the namespace.

::

      git clone https://github.com/mochi-hpc/mochi-spack-packages.git <MOCHI_TARGET_DIR>
      spack repo add <MOCHI_TARGET_DIR>

Build and install the necessary mochi dependencies.

::

      spack install --add mochi-thallium mochi-margo mercury argobots 



Clone the Copper github repository and move into the cloned
directory.

::

      git clone https://github.com/argonne-lcf/copper <COPPER_TARGET_DIR>
      cd <COPPER_TARGET_DIR>

Copy the file ``<TARGET_DIR>/scripts/default_env.sh`` new file named
``<COPPER_TARGET_DIR>/scripts/env.sh`` and set up the necessary env vars.
::

      cp <COPPER_TARGET_DIR>/scripts/default_env.sh <COPPER_TARGET_DIR>/scripts/env.sh

The variables necessary for building and running are listed below:

- FUSE3_LIB: folder containing the libfuse library
- FUSE3_INCLUDE: folder containing the ``fuse.h`` header
- VIEW_DIR: Copper folder access point

Building
^^^^^^^^^^^^^^^^^^^^^^

The following env vars if changed require a recompilation:

- BLOCK_REDUNDANT_RPCS

The following command assumes ``cmake`` is available.

::

      sh <COPPER_TARGET_DIR>/build_helper/build.sh

Running
^^^^^^^^^^^^^^^^^^^^^^

The following env vars will take affect without recompilation:

- LOG_LEVEL: 0 is most verbose (i.e., trace) and 6 is least verbose (i.e., fatal error)
- LOG_TYPE: where stdout/stderr should be sent options are ``file``, ``stdout``, or ``file_and_stdout``
- LOG_OUTPUT_DIR: folder which will hold the output from each Copper background process
- NET_TYPE: options are ``cxi``, ``tcp``, or ``na+sm``
- ADDR_WRITE_SYNC_TIME: time to wait for Copper background RPC servers to sync not necessary if address book is available
- SINGLE_THREADED: whether FUSE should be launched with mult-threaded or single-threaded mode
- NODE_FILE: location of cxi address book

Currently, running copper requires the user to launch the Copper background
process on each node, then running the user application, and finally shutting
down copper after the user application ends. In addition, the <LOG_OUTPUT_DIR>
must be removed before each Copper launch.

::

      rm -r <LOG_OUTPUT_DIR>
      mpirun --cpu-bind="list:4,8,12,16" -np <NNODES> -ppn 1 <COPPER_TARGET_DIR>/scripts/filesystem/mnt.sh
      # launch user application
      mpirun -np <NNODES> -ppn 1 fusermount3 -u <COPPER_TARGET_DIR>/scripts/filesystem/umnt.sh

.. toctree::
      :maxdepth: 2
      :caption: Contents:

