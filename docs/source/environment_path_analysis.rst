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

``LD_LIBRARY_PATH``
   Controls shared-library search order for compiled extensions and ROCm
   libraries. Duplicate or stale entries here can create large probe storms.

``srun --export=ALL``
   Replicates the activated environment across all ranks, which is necessary
   for correctness but also multiplies import and loader discovery activity.

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
