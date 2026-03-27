Aurora and Frontier
===================

Copper is commonly deployed on both Aurora and Frontier. The scheduler,
service-core bindings, mount paths, and job launch commands differ slightly by
site, so this page provides platform-specific build notes, runtime usage, and
example locations.

Platform Quick Reference
------------------------

.. list-table::
   :header-rows: 1

   * - Platform
     - Scheduler
     - Common module load
     - Common endpoint family
     - Common Copper service cores
   * - Aurora
     - PBS
     - ``module load copper``
     - ``cxi`` / ``cxi0``
     - ``48,49,50,51``
   * - Frontier
     - Slurm
     - ``module load ums ums046 copper``
     - ``cxi://cxi1``
     - ``1,2`` or ``1,2,65,66``

Starting and Stopping Copper
----------------------------

Platform-specific wrappers are used on each system:

.. code-block:: bash

   launch_copper_aurora.sh   [-d log_dir_base] [-v CU_FUSE_MNT_VIEWDIR]
   stop_copper_aurora.sh     [-d log_dir_base] [-v CU_FUSE_MNT_VIEWDIR]
   launch_copper_frontier.sh [-d log_dir_base] [-v CU_FUSE_MNT_VIEWDIR]
   stop_copper_frontier.sh   [-d log_dir_base] [-v CU_FUSE_MNT_VIEWDIR]

Aurora Build Notes
------------------

Aurora deployments typically use the site ``copper`` module for runtime use.
For source builds, activate the appropriate Mochi dependency environment,
update ``scripts/env.sh`` for the Aurora toolchain and paths, and run the build
helper.

.. code-block:: bash

   module load cmake
   module load gcc
   source /path/to/spack/share/spack/setup-env.sh
   spack env activate spack-copper-mod-env

   cd /path/to/copper/scripts
   cp default_env.sh env.sh
   sh build_helper/build.sh

Aurora Usage Example
--------------------

.. code-block:: bash

   module load copper
   APP_BASE=/lus/flare/projects/datascience/${USER}/exp1
   MY_COPPER_MOUNT=/tmp/${USER}/copper_mount

   ./build/launch_copper_aurora.sh -d ${APP_BASE}/copper-logs-dir -v ${MY_COPPER_MOUNT}

   time mpirun --np ${NRANKS} --ppn ${RANKS_PER_NODE} \
     --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 --genvall \
     --genv=PYTHONPATH=${MY_COPPER_MOUNT}${APP_BASE}/lus_custom_pip_env:$PYTHONPATH \
     python3 -c "import torch; print(torch.__file__)"

   ./build/stop_copper_aurora.sh -d ${APP_BASE}/copper-logs-dir -v ${MY_COPPER_MOUNT}

Aurora Example Files
--------------------

The Aurora example scripts are available in:

- ``examples/aurora_examples/launch_copper_aurora.sh``
- ``examples/aurora_examples/stop_copper_aurora.sh``
- ``examples/aurora_examples/simple_with_copper.sh``
- ``examples/aurora_examples/simple_without_copper.sh``

These Aurora example wrappers are standalone and use PBS-oriented launch logic.
They are intended to be readable and adaptable without depending on the
generic wrapper script.

After build, the default Aurora facility address-book path used by the
wrappers is ``build/alcf_aurora_copper_addressbook.txt``.

Frontier Build Notes
--------------------

Frontier deployments commonly use the UMS-managed Copper environment. For
source builds, activate the Frontier Spack environment, review ``scripts/env.sh``,
and run the build helper.

.. code-block:: bash

   module load gcc-native/14.2
   module load cmake
   source /sw/frontier/ums/ums046/spack/share/spack/setup-env.sh
   spack env activate spack-copper-mod-env

   cd /path/to/copper/scripts
   cp default_env.sh env.sh
   sh build_helper/build.sh

Frontier Usage Example
----------------------

.. code-block:: bash

   module load ums ums046 copper
   APP_BASE=/lustre/orion/proj-shared/ums046/${USER}/exp1
   MY_COPPER_MOUNT=/mnt/bb/$USER/copper_mount

   ./build/launch_copper_frontier.sh -d ${APP_BASE}/copper-logs-dir -v ${MY_COPPER_MOUNT}
   conda activate ${MY_COPPER_MOUNT}${APP_BASE}/conda_env
   CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73

   /usr/bin/time srun --overlap -N ${SLURM_NNODES} -n $((SLURM_NNODES * 8)) \
     --ntasks-per-node=8 --cpu-bind=${CPU_BINDING_MAP} \
     python3 -c "import torch; print('torch imported from:', torch.__file__)"

   ./build/stop_copper_frontier.sh -d ${APP_BASE}/copper-logs-dir -v ${MY_COPPER_MOUNT}

Frontier Example Files
----------------------

The Frontier example scripts are available in:

- ``examples/frontier_examples/launch_copper_frontier.sh``
- ``examples/frontier_examples/stop_copper_frontier.sh``
- ``examples/frontier_examples/simple_with_copper.sh``
- ``examples/frontier_examples/simple_without_copper.sh``

These Frontier example wrappers are standalone and use Slurm-oriented launch
logic. They are intended to be readable and adaptable without depending on the
generic wrapper script.

After build, the default Frontier facility address-book path used by the
wrappers is ``build/olcf_frontier_copper_addressbook.txt``.

Related Pages
-------------

- :doc:`guide_overview_and_best_practices`
- :doc:`guide_build_and_run`
- :doc:`guide_build_artifacts_and_platforms`
