Deep Debugging
==============

Purpose
-------

This page records the recommended deep-debugging workflow for issues that are
not visible through normal production logging.

Core File Collection
--------------------

To enable core file generation for ``cu_fuse`` ranks, set the core-file limit
before ``exec`` and record the relevant process state:

.. code-block:: bash

   ulimit -c unlimited
   echo "core_ulimit=$(ulimit -c)"
   echo "core_pattern=$(cat /proc/sys/kernel/core_pattern)"

Additional Runtime Debug Signals
--------------------------------

For communication-layer debugging, the following environment variables are the
most useful:

.. code-block:: bash

   export FI_LOG_LEVEL=debug
   export FI_LOG_PROV=all
   export HG_LOG_LEVEL=debug
   export HG_LOG_SUBSYS=hg,na,libfabric

Symbol-Rich Builds
------------------

When a stack trace or postmortem analysis is needed, use a symbol-rich build:

.. code-block:: bash

   module load gcc-native/14.2
   module load cmake
   source /sw/frontier/ums/ums046/spack/share/spack/setup-env.sh
   spack env activate spack-copper-mod-env
   cd /path/to/copper/scripts
   BUILD_TYPE=RelWithDebInfo sh ./build_helper/build.sh

Recommended Evidence Collection
-------------------------------

For startup or mount failures, preserve:

- the main Slurm or PBS stdout file
- per-rank ``*-output.log`` files
- ``logs/copper_address_book.txt``
- ``logs/copper_address_book_full_output.txt`` when ``discover`` mode is used
- any generated core files

Debugging Principle
-------------------

Prefer targeted, minimally invasive instrumentation and preserve the first
failure signal. Secondary cleanup failures often hide the original issue.
