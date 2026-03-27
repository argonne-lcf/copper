Build and Run
=============

Build Requirements
------------------

Copper depends on the Mochi communication stack and a working FUSE3
installation. A typical build environment provides:

- ``cmake``
- an MPI compiler toolchain
- ``margo``
- ``mercury``
- ``thallium``
- ``argobots``
- ``cereal``

The site environment may be provided through modules, Spack, or both.

Build Procedure
---------------

From the repository root:

.. code-block:: bash

   cd /path/to/copper/scripts
   cp default_env.sh env.sh
   sh build_helper/build.sh

Expected artifacts after a successful build include:

- ``build/cu_fuse``
- ``build/cu_fuse_shutdown``
- ``build/list_cxi_hsn_thallium``
- ``build/launch_copper_aurora.sh``
- ``build/launch_copper_frontier.sh``
- ``build/stop_copper_aurora.sh``
- ``build/stop_copper_frontier.sh``
- ``build/olcf_frontier_copper_addressbook.txt``
- ``build/alcf_aurora_copper_addressbook.txt``

For platform-specific build and runtime examples, see
:doc:`guide_aurora_and_frontier`.

Runtime Model
-------------

Copper is launched as one background ``cu_fuse`` process per node, then the
user workload is started against the Copper mount, and finally the Copper
processes are shut down.

The standard launch flow adds two important startup behaviors:

- preparation of a job-local address book before the full Copper launch
- cached parent-readiness checking before the first forwarded parent RPC

The normal retained startup timing signals are:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

Launch Example
--------------

.. code-block:: bash

   module load ums ums046 copper
   launch_copper_frontier.sh -d /path/to/logdir -v /mnt/bb/$USER/copper_mount
   conda activate /mnt/bb/$USER/copper_mount/lustre/orion/.../conda_env
   python3 -c "import torch"
   stop_copper_frontier.sh -d /path/to/logdir -v /mnt/bb/$USER/copper_mount

Platform-specific launch examples are also provided under:

- ``examples/aurora_examples``
- ``examples/frontier_examples``

Personal Conda Environments
---------------------------

When using a personal Conda environment through Copper, prepend
``/tmp/${USER}/copper/`` only to the path passed to ``conda activate``. The
environment itself does not need to be rebuilt or relocated.

For example:

.. code-block:: bash

   conda activate /tmp/${USER}/copper/lustre/orion/.../conda_env

Profiling Controls
------------------

Copper supports the following profiling interface:

- ``-profile_metrics``
- ``-profile_top_n <N>``
- ``-profile_paths_full``
- ``-profile_snapshot_interval_s <seconds>``

The wrapper equivalents are:

- ``-P``
- ``-N <N>``
- ``-A``
- ``-I <seconds>``

Address-Book Source Modes
-------------------------

Two source modes are available in the launch wrapper:

``facility``
   Filters a provided facility address book down to the current allocation.

``discover``
   Runs ``list_cxi_hsn_thallium`` across the allocation, preserves the raw
   discovery output, and derives the final ``copper_address_book.txt`` from the
   endpoint column selected by ``net_type``.

When ``discover`` mode is used, the wrapper preserves:

- ``logs/copper_address_book.txt``
- ``logs/copper_address_book_full_output.txt``

In normal ``facility`` mode, the runtime wrappers default to the build-staged
facility files:

- ``build/olcf_frontier_copper_addressbook.txt``
- ``build/alcf_aurora_copper_addressbook.txt``

The source copies remain under ``scripts/`` and are copied into ``build/``
after compilation.
