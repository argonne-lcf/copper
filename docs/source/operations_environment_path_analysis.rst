Environment Path Analysis
=========================

Scope
-----

This page integrates the maintained findings from the PyTorch dependency and
environment-path evaluations. The source material came from:

- the 2-node profiling evaluation with ``python3 -c "import torch"``
- the environment-path analysis for the same workload
- the full-path profiling evaluation in the final experiment set

The goal is to explain why a simple import touches so many files and how to
reduce unnecessary path fan-out without breaking the active runtime.

Workload Context
----------------

The maintained conclusions come from profiling-enabled runs that launched a
Python environment through Copper and then executed a minimal
``python3 -c "import torch"`` workload. Although the user-visible workload is
small, the runtime behavior is not. The interpreter, import system, package
manager environment, dynamic loader, and native Torch dependency graph all
participate in the startup path.

The corresponding profiling outputs showed the mounted environment dominating
the lookup stream, especially under:

- the environment root itself
- ``lib/``
- ``lib/python3.12``
- ``lib/python3.12/site-packages``
- ``site-packages/torch``

The maintained iter3 artifacts under ``docs/source/iter3`` also preserved a
full-path usage analysis for the active Conda environment. That analysis is
useful because it complements the hot-path tables with a coarse "used versus
available" estimate for the exact same workload family.

Why ``import torch`` Touches So Many Paths
-----------------------------------------

The import is simple at the Python source level, but not at the runtime level.
In one launch, the following all happen before user code does meaningful work:

- Python interpreter startup
- import-system path discovery under ``lib/python*`` and ``site-packages``
- package discovery inside ``torch`` and its transitive imports
- dynamic-loader resolution for compiled extension modules
- ROCm shared-library loading
- repeated probes for optional or absent libraries and helper paths

In practice, the observed path fan-out is the combined effect of:

- the selected ``python3`` executable from ``PATH``
- Conda activation variables such as ``CONDA_PREFIX``
- Python import search rules and ``site.py`` processing
- ``LD_LIBRARY_PATH`` search behavior for native libraries
- PyTorch's compiled ROCm dependency graph
- repeated missing-path probes that are normal for loader startup

This is why a nominally simple import often fans out into:

- Python interpreter startup work
- standard-library discovery
- ``site.py`` processing
- package-directory walks inside ``site-packages``
- import of many Torch Python subpackages
- loading of compiled extension modules
- dynamic-loader resolution of large native dependency sets
- optional-library probing that is expected to fail in many cases

Observed Path Classes
---------------------

The full-path profiling run showed the hottest filesystem classes clearly.
Across the four-rank cluster summary, the dominant classes were:

.. list-table::
   :header-rows: 1

   * - Path class
     - Total events
     - Meaning
   * - ``environment_prefix``
     - ``5,656,710``
     - broad activity under the environment root and its parent directories
   * - ``python_stdlib``
     - ``394,464``
     - interpreter startup and stdlib discovery
   * - ``torch_python_package``
     - ``377,216``
     - Python-side Torch package import activity
   * - ``python_site_packages``
     - ``327,640``
     - package-discovery traffic in the active environment
   * - ``torch_native_library``
     - ``180,297``
     - compiled Torch and ROCm libraries loaded during startup
   * - ``missing_shared_library_probe``
     - ``2,180``
     - optional or absent shared-library probes

Representative missing-path classes included:

- ``libhsa-amd-aqlprofile64.so``
- ``python312.zip``
- ``glibc-hwcaps``
- ``pyvenv.cfg``

These are usually normal probes rather than application bugs.

The profiling notes also showed heavy data reads from native libraries such as:

- ``libtorch_cpu.so``
- ``libtorch_python.so``
- ``libamdhip64.so``
- ``libMIOpen.so``
- ``libmagma.so``
- ``librocblas.so``
- ``librocsolver.so``
- ``librocsparse.so``

That pattern is consistent with a large GPU-enabled Torch stack rather than a
small pure-Python package import.

Path Coverage in the Iter3 Environment Copy
-------------------------------------------

The iter3 path-usage analysis compared the observed full-path outputs against
the full existing path universe under the selected Conda environment root:

.. list-table::
   :header-rows: 1

   * - Measure
     - Value
   * - All existing paths under the selected root
     - ``44,262``
   * - Existing files under the selected root
     - ``42,028``
   * - Existing directories under the selected root
     - ``2,234``
   * - Existing paths observed in the run
     - ``2,350``
   * - Missing probe paths
     - ``559``
   * - Existing paths not observed in the run
     - ``41,912``

The same summary expressed that as coverage of the selected root:

.. list-table::
   :header-rows: 1

   * - Coverage metric
     - Value
   * - Observed files
     - ``1,982`` of ``42,028``
   * - File coverage
     - ``4.72%``
   * - Observed directories
     - ``368`` of ``2,235``
   * - Directory coverage
     - ``16.47%``

That result is useful, but it needs to be interpreted carefully. It does not
mean the remaining roughly ``95%`` of files are safe to delete in general.
It means only that, in this same-app, same-node-count, same-configuration run,
the observed import path touched a relatively small fraction of the total
environment tree.

Operationally, the main value of this result is:

- it shows that the active workload depends on a minority of the available
  file tree during this exact startup path
- it supports using observed paths as an initial allowlist for cloned or
  filtered follow-up experiments
- it argues for evidence-driven pruning rather than assuming the whole
  environment is equally active

Environment Variables That Matter Most
--------------------------------------

``PATH``
   Chooses which ``python3`` is launched. Once the interpreter comes from the
   Conda environment mounted through Copper, many later paths are derived from
   that prefix automatically.

``CONDA_PREFIX``
   Anchors the active environment root, including ``bin``, ``lib``, and
   ``lib/python*/site-packages``.

``PYTHONPATH``
   Adds optional import roots. It is important, but it is not the whole story;
   Python still derives a large built-in search path from the interpreter and
   its install prefix.

``VIRTUAL_ENV``
   Is often not the main driver for Conda-based runs, but Python still probes
   for virtual-environment style markers such as ``pyvenv.cfg`` while
   establishing its runtime layout.

``LD_LIBRARY_PATH``
   Controls shared-library search order for compiled extensions and ROCm
   libraries. Duplicate or stale entries here can create large probe storms.

``srun --export=ALL``
   Replicates the activated environment across all ranks, which is necessary
   for correctness but also multiplies import and loader discovery activity.

Path Sources by Subsystem
-------------------------

Different path classes come from different subsystems, so path reduction works
best when those subsystems are considered separately.

Python import machinery contributes:

- interpreter-prefix discovery
- stdlib and ``lib-dynload`` walks
- ``site-packages`` scanning
- package and subpackage traversal for ``torch`` and its transitive imports

Environment activation contributes:

- active environment prefixes from ``CONDA_PREFIX`` and related variables
- path insertion in ``PATH``
- optional import roots from ``PYTHONPATH``
- propagated shell state when tasks are launched with full environment export

The dynamic loader contributes:

- shared-library searches under the active environment
- probing across ``LD_LIBRARY_PATH`` entries
- optional-library probes for features that may not be installed
- hardware-capability directory probes such as ``glibc-hwcaps``

The iter3 path-class summary is consistent with that subsystem view. The
largest path classes were:

- ``environment_prefix`` with ``2,828,400`` events
- ``python_stdlib`` with ``197,232`` events
- ``torch_python_package`` with ``188,608`` events
- ``python_site_packages`` with ``163,484`` events
- ``torch_native_library`` with ``57,503`` events

Those totals show that most activity remains concentrated in a small set of
environment, interpreter, package, and native-library regions rather than
being evenly distributed across the full environment tree.

Why Missing Paths Repeat
------------------------

The profiling data shows many repeated negative probes. This is expected for
Python and shared-library startup:

- the first lookup discovers a path is absent
- later lookups ask for the same exact path again
- Copper can serve that repeated miss from the metadata ``ENOENT`` TTL

This is why high TTL-serve counts are a positive signal. They mean Copper is
collapsing repeated negative metadata work that the workload would otherwise
reissue.

The version4 path-analysis note highlighted several repeated examples:

- ``libhsa-amd-aqlprofile64.so``
- ``python312.zip``
- ``glibc-hwcaps``
- ``pyvenv.cfg``

The iter3 artifacts preserved the same pattern in both the TTL top-path tables
and the missing-probe lists. Representative repeated probe paths included:

- ``.../torch/lib/libhsa-amd-aqlprofile64.so``
- ``.../lib/python312.zip``
- ``.../lib/glibc-hwcaps``
- ``.../conda_env/pyvenv.cfg``
- ``.../conda_env/bin/pyvenv.cfg``

These should generally be interpreted as normal startup probes first and
optimization opportunities second.

Pruning and Cleanup Guidance
----------------------------

The safest cleanup sequence is:

#. remove duplicate path entries first
#. remove obviously nonexistent path entries
#. remove stale environment or toolchain directories
#. only then experiment with a reduced or allowlist-based environment copy

The environment-path and full-path profiling evaluations support the following
practical rules:

- keep the active environment core intact first:
  environment root, ``bin``, ``lib``, ``lib/python*``, and ``site-packages``
- prefer trimming duplicate or stale ``LD_LIBRARY_PATH`` entries before
  touching Torch library directories
- prefer trimming duplicate or unnecessary ``PYTHONPATH`` additions before
  modifying the interpreter tree
- treat ``python*.zip``, ``pyvenv.cfg``, and ``glibc-hwcaps`` as optimization
  hints, not as correctness failures

Minimization Priorities
-----------------------

The maintained guidance from the path-analysis work is to minimize the active
environment in layers rather than trying to remove all path fan-out at once.

The safest order is:

#. eliminate duplicate path entries
#. eliminate obviously nonexistent path entries
#. remove stale toolchain or environment references that are no longer active
#. preserve the active runtime core while measuring again
#. only then consider more aggressive allowlist-style environment reduction

This approach keeps the debugging loop tied to observed profiling evidence
instead of guessing which paths are safe to remove.

Operational Interpretation
--------------------------

The right question is usually not "why is Python probing so many files?" but
"which of those probes are avoidable in the active environment?"

The maintained guidance from these evaluations is:

- keep the active environment small and purpose-built
- route only the necessary environment prefixes through Copper
- keep the metadata ``ENOENT`` TTL enabled
- use profiling outputs to identify duplicate, stale, or noisy environment
  paths before changing package contents
