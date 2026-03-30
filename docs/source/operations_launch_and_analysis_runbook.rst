Launch and Analysis Runbook
===========================

Purpose
-------

This page is a step-by-step runbook for common Copper launch and post-run
analysis tasks. It is intentionally procedural, even where some of the same
material appears elsewhere in the documentation set.

Use this page when the goal is to:

- launch Copper with basic runtime settings
- enable startup timing visibility
- enable profiling outputs
- choose between facility and discovery address-book preparation
- run the post-processing scripts after a job completes

Step 1: Choose a Log Level
--------------------------

Copper logging is controlled with ``-l`` in the platform launch wrappers and
with ``-log_level`` in direct ``cu_fuse`` runs.

The current user-facing levels are:

.. list-table::
   :header-rows: 1

   * - Level
     - Meaning
     - Typical use
   * - ``-l 0``
     - no logging
     - quiet production runs
   * - ``-l 1``
     - fatal only
     - crash-only visibility
   * - ``-l 2``
     - error and fatal
     - error-focused troubleshooting
   * - ``-l 3``
     - warning, error, and fatal
     - warning-focused troubleshooting
   * - ``-l 4``
     - info, warning, error, and fatal
     - startup timing and readiness studies
   * - ``-l 5``
     - debug-heavy / most logging
     - deep debugging

If the goal is to retain the compact startup timing lines:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

use at least:

- ``-l 4``

Step 2: Choose an Address-Book Source
-------------------------------------

Copper supports two address-book source modes in the launch wrappers.

``facility``
   Filters a provided facility address book down to the current allocation.

``discover``
   Runs the discovery helper across the allocation, preserves the raw output,
   and derives the final ``copper_address_book.txt`` from the selected network
   column.

Practical guidance:

- use ``facility`` when the site address book is trusted and current
- use ``discover`` when a fresh allocation-derived mapping is needed
- when using ``discover``, keep both:
  - ``logs/copper_address_book.txt``
  - ``logs/copper_address_book_full_output.txt``

Step 3: Choose Profiling Options
--------------------------------

Copper profiling can be enabled with any combination of the following options:

.. list-table::
   :header-rows: 1

   * - Wrapper option
     - Effect
   * - ``-P``
     - enable profiling collection
   * - ``-N <N>``
     - enable profiling and keep the hottest ``N`` paths
   * - ``-A``
     - enable fuller path-level outputs
   * - ``-I <seconds>``
     - enable periodic profiling snapshots while the job is still running

The corresponding direct ``cu_fuse`` options are:

- ``-profile_metrics``
- ``-profile_top_n <N>``
- ``-profile_paths_full``
- ``-profile_snapshot_interval_s <seconds>``

Common combinations:

- no profiling:
  - no ``-P``, ``-N``, ``-A``, or ``-I``
- aggregate profiling:
  - ``-P``
- bounded hotspot profiling:
  - ``-P -N 20``
- full-path forensic profiling:
  - ``-P -N 20 -A``

Step 4: Launch Copper
---------------------

Aurora
^^^^^^

Basic run with startup timing visibility:

.. code-block:: bash

   launch_copper_aurora.sh -l 4 -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Facility-mode run with profiling enabled:

.. code-block:: bash

   launch_copper_aurora.sh -l 4 -P -N 20 -A -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Discovery-mode run:

.. code-block:: bash

   launch_copper_aurora.sh -l 4 -a discover -n cxi -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Aurora notes:

- ``cxi`` is the common endpoint family in the Aurora wrappers
- ``48,49,50,51`` is the common Copper service-core example
- ``-l 4`` is the recommended setting when startup timing matters

Frontier
^^^^^^^^

Basic run with startup timing visibility:

.. code-block:: bash

   launch_copper_frontier.sh -l 4 -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Facility-mode run with profiling enabled:

.. code-block:: bash

   launch_copper_frontier.sh -l 4 -P -N 20 -A -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Discovery-mode run:

.. code-block:: bash

   launch_copper_frontier.sh -l 4 -a discover -n cxi://cxi1 -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Frontier notes:

- ``cxi://cxi1`` is a common endpoint example
- ``1,2`` or ``1,2,65,66`` are common Copper service-core examples

Step 5: Run the Workload
------------------------

After Copper starts, run the workload against the Copper-mounted path.

Aurora example:

.. code-block:: bash

   time mpirun --np "${NRANKS}" --ppn "${RANKS_PER_NODE}" \
     --cpu-bind=list:4:56:9:61:14:66:19:71:20:74:25:79 \
     --genvall \
     --genv=PYTHONPATH="${MY_COPPER_MOUNT}${PACKAGE_DIR}/:${PYTHONPATH}" \
     python3 -c "import torch; print(torch.__file__);"

Frontier example:

.. code-block:: bash

   /usr/bin/time srun --overlap -N "${SLURM_NNODES}" -n $((SLURM_NNODES * 8)) \
     --ntasks-per-node=8 --cpu-bind="${CPU_BINDING_MAP}" \
     python3 -c "import torch; print(torch.__file__)"

Step 6: Stop Copper
-------------------

Aurora:

.. code-block:: bash

   stop_copper_aurora.sh -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Frontier:

.. code-block:: bash

   stop_copper_frontier.sh -d "${LOGDIR}" -v "${MY_COPPER_MOUNT}"

Important shutdown note:

- run the matching ``stop_copper_aurora.sh`` or ``stop_copper_frontier.sh``
  before attempting ``aggregate_profiling.py`` or
  ``path_usage_analysis.py``
- wait for Copper shutdown to complete on all nodes before running the
  analysis scripts
- the final profiling CSV files under ``profiling/final/`` and the final raw
  table outputs under ``tables/final/`` may not exist until shutdown has
  completed
- if analysis is attempted too early, the common failure mode is:
  ``failed to find profiling csv files under: .../profiling/final``

Step 7: Locate the Job Output Directory
---------------------------------------

The post-processing scripts expect the Copper job root directory, which is the
directory that contains:

- ``logs/``
- ``tables/``
- ``profiling/``

Typical example:

.. code-block:: text

   /path/to/copper-logs-dir/<jobid>

Do not point the scripts at:

- ``logs/`` directly
- ``profiling/final/`` directly
- ``profiling/cluster/`` directly

Step 8: Run Aggregate Profiling
-------------------------------

The aggregate profiling script combines per-rank profiling outputs into
cluster-level summaries.

Basic usage:

.. code-block:: bash

   cd /lus/flare/projects/datascience/kaushik/copper-tests/copper/scripts
   python3 aggregate_profiling.py /path/to/copper-logs-dir/<jobid>

With a custom output prefix:

.. code-block:: bash

   python3 aggregate_profiling.py /path/to/copper-logs-dir/<jobid> cluster_usage_test

With explicit usage-root analysis:

.. code-block:: bash

   python3 aggregate_profiling.py \
     /path/to/copper-logs-dir/<jobid> \
     cluster_usage_test \
     --usage-root /path/to/package-root

Outputs are written under:

- ``profiling/cluster/<prefix>-profiling_aggregate.csv``
- ``profiling/cluster/<prefix>-profiling_operations.csv``
- ``profiling/cluster/<prefix>-profiling_summary.md``

If the script reports:

``failed to find profiling csv files under: ...``

the most common causes are:

- the run was not launched with profiling enabled
- the wrong job root directory was passed
- Copper had not been stopped yet, so the final outputs had not been flushed
- the run failed before profiling files were written

Step 9: Inspect Cache Table Usage
---------------------------------

The ioctl helper scripts can also capture current table occupancy for Copper's
three main cache tables and summarize them into one combined report.

Workflow:

.. code-block:: bash

   cd /path/to/copper/scripts/get_copper_stats_ioctl

   # Step 1: add this to env.sh, or export it in the current shell
   export VIEW_DIR=/mnt/bb/${USER}/copper_mount

   # Step 2: while Copper is still running, collect the raw cache-size outputs
   bash ./get_cache_usage_summary.sh /lustre/orion/proj-shared/ums046/some_output_dir

   # Step 3: for offline re-summarization of an existing directory
   python3 ./summarize_cache_usage.py /path/to/dir --csv /path/to/dir/cache_usage_summary.csv

Outputs include:

- ``data_cache_size.output``
- ``tree_cache_size.output``
- ``md_cache_size.output``
- ``cache_usage_summary.txt``
- ``cache_usage_summary.csv``

Interpretation:

- the reports show current used bytes and entry counts
- the combined summary shows the total currently occupied bytes across all
  three tables
- the tables are dynamically growing ``unordered_map`` containers rather than
  fixed-capacity pools
- memory usage is bounded by normal process and node limits rather than by a
  Copper-specific global table budget
- ``max_cacheable_byte_size`` is a per-file admission threshold for data
  caching, not a total cache budget
- Copper does not yet report a fixed total budget or remaining available bytes
  for those tables

Sample output:

.. code-block:: md

   | Table    | Files Found | Used Bytes | Used Human | Entries |
   | -------- | ----------: | ---------: | ---------: | ------: |
   | data     | 1           | 70296558   | 67.04 MiB  | 1025    |
   | tree     | 1           | 27772      | 27.12 KiB  | 140     |
   | metadata | 1           | 340848     | 332.86 KiB | 2367    |
   | combined | -           | 70665178   | 67.39 MiB  | 3532    |

Step 10: Run Path Usage Analysis
--------------------------------

The path-usage script compares observed paths against the selected filesystem
roots and produces used-path, missing-probe, and candidate-not-observed lists.

Basic usage:

.. code-block:: bash

   cd /lus/flare/projects/datascience/kaushik/copper-tests/copper/scripts
   python3 path_usage_analysis.py /path/to/copper-logs-dir/<jobid>

With an explicit package root:

.. code-block:: bash

   python3 path_usage_analysis.py \
     /path/to/copper-logs-dir/<jobid> \
     --usage-root /path/to/package-root

With a custom output directory:

.. code-block:: bash

   python3 path_usage_analysis.py \
     /path/to/copper-logs-dir/<jobid> \
     --usage-root /path/to/package-root \
     --output-dir /path/to/copper-logs-dir/<jobid>/paths_dir

Outputs include:

- ``all_possible_existing_paths.txt``
- ``used_paths_existing.txt``
- ``same_run_candidate_not_observed_existing_paths.txt``
- ``missing_probe_paths.txt``
- ``roots_and_counts.txt``
- ``paths_summary.md``

Sample Outputs from Aurora Run 8405873
--------------------------------------

The examples below come from:

- ``/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/scale_test/2_nodes/copper-logs-dir/8405873``

Startup log sample
^^^^^^^^^^^^^^^^^^

From ``logs/x4613c3s7b0n0-82226-output.log``:

.. code-block:: text

   2026-03-27 23-33-57.994 [Info] (cu_hello_main) user-facing log_level: 5
   2026-03-27 23-33-57.994 [Info] (cu_hello_main) profile_metrics: enabled
   2026-03-27 23-33-57.994 [Info] (cu_hello_main) profile_top_n: 20
   2026-03-27 23-33-57.994 [Info] (cu_hello_main) profile_paths_full: enabled
   2026-03-27 23-33-57.997 [Info] (start_thallium_engine) starting thallium engine
   2026-03-27 23-33-58.078 [Info] (start_thallium_engine) engine started
   2026-03-27 23-33-58.082 [Info] (start_thallium_engine) server running at address: ofi+cxi://0x09470400
   2026-03-27 23-33-58.082 [Info] (start_thallium_engine) provider registration completed after 85096 us

This startup view confirms:

- the chosen log level
- whether profiling was enabled
- whether the server came up cleanly
- the local provider registration time

Cluster aggregate summary sample
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From ``profiling/cluster/cluster_usage_test-profiling_summary.md``:

.. code-block:: text

   Ranks with aggregate files: 4
   total_cache_hits: 5984151
   total_cache_misses: 3010
   total_counted_fuse_operations: 6377844
   total_measured_latency_seconds: 39.513989
   total_negative_results: 168147
   metadata_enoent_ttl_serves: 45424
   metadata_enoent_ttl_stores: 2372

Selected operation totals from the same summary:

.. code-block:: text

   getattr: 5952916 calls, 31.390461 s total latency
   read: 195912 calls, 7.008104 s total latency
   readdir: 6480 calls, 0.535788 s total latency

This summary is useful for:

- confirming that cluster aggregation succeeded
- identifying the hottest operations by call count
- identifying the operations that dominated measured latency
- checking how heavily Copper served data from cache

Path usage summary sample
^^^^^^^^^^^^^^^^^^^^^^^^^

From ``paths_dir/paths_summary.md``:

.. code-block:: text

   All possible existing paths under the selected roots: 22383
   Actually used in this run: 1729
   Probed but absent: 555
   Observed files: 1404 of 20625 (6.81%)
   Observed directories: 325 of 1758 (18.49%)

This path summary supports a staged interpretation:

- the selected environment tree was much larger than the set touched by the
  run
- file coverage for this run was 6.81 percent, not full-tree usage
- missing probe paths are normal loader or Python search misses and should not
  be confused with active deletion

Path class sample
^^^^^^^^^^^^^^^^^

Also from ``profiling/cluster/cluster_usage_test-profiling_summary.md``:

.. code-block:: text

   environment_prefix: 5075764 events, example /lus
   shared_library: 126484 events, example .../nvidia/cu13/lib/libcublasLt.so.13
   negative_probe_path: 19452 events, example .../aurora_runs/nvidia

These classes are useful when planning cleanup experiments:

- keep the active environment core paths first
- review repeated negative probe paths for stale search-path entries
- use the observed set as an allowlist candidate for a cloned environment,
  rather than as proof that unseen files are permanently unnecessary

Practical Reading Order
-----------------------

If the goal is fast post-run interpretation, the simplest reading order is:

#. check the Copper node logs in ``logs/`` for startup and readiness timing
#. check ``profiling/cluster/*.md`` for aggregate workload shape
#. check ``profiling/cluster/*.csv`` for totals and per-operation counts
#. check ``paths_summary.md`` when pruning or environment minimization is the
   next question

Related Pages
-------------

- :doc:`guide_aurora_and_frontier`
- :doc:`guide_build_and_run`
- :doc:`operations_profiling_overview`
- :doc:`operations_profiling_reference`
- :doc:`operations_environment_path_analysis`
