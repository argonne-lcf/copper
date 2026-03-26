Building and Platforms
======================

Build Model
-----------

Copper uses a straightforward build model:

- CMake-based build
- Mochi dependencies from modules or Spack
- wrapper scripts and shipped address-book files copied into the build directory after build

Important Binaries
------------------

After build, the primary expected artifacts are:

- ``build/cu_fuse``
- ``build/cu_fuse_shutdown``
- ``build/list_cxi_hsn_thallium``
- ``build/launch_copper.sh``
- ``build/stop_copper.sh``
- ``build/olcf_frontier_copper_addressbook.txt``
- ``build/alcf_aurora_copper_addressbook.txt``

Platform Notes
--------------

Frontier
^^^^^^^^

- scheduler: Slurm
- common runtime module load: ``module load ums ums046 copper``
- wrapper examples use ``srun``
- common Copper endpoint example: ``cxi://cxi1``
- common Copper service-core example: ``1,2``; some deployments may also use ``1,2,65,66``
- example scripts: ``examples/frontier_examples``

Aurora
^^^^^^

- scheduler: PBS
- common runtime module load: ``module load copper``
- wrapper examples use ``mpirun`` for workload launch
- common Copper endpoint example: ``cxi`` / ``cxi0``
- common Copper service-core example: ``48,49,50,51``
- example scripts: ``examples/aurora_examples``

Address-Book Source Modes
-------------------------

``facility``
   Filter a provided facility address book down to the current job nodes.

``discover``
   Run ``list_cxi_hsn_thallium`` across the allocation, preserve the raw
   multi-column output, and derive the final Copper address-book file from the
   column selected by ``net_type``.

Local Reference Files
---------------------

- ``README.md``
- ``docs/source/aurora_and_frontier.rst``
- ``docs/source/building_and_running.rst``
- ``scripts/olcf_frontier_copper_addressbook.txt``
- ``scripts/alcf_aurora_copper_addressbook.txt``
- ``copper-tests/copper-get-addressbook/list_cxi_hsn_thallium.cpp``
- ``copper-tests/README.md``
