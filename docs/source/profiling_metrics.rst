Profiling Metrics Reference
===========================

Profiling Modes
---------------

Copper supports three profiling levels:

Aggregate-only profiling
   Records cluster-wide totals and per-operation summaries.

Top-path profiling
   Records the hottest paths per category, bounded by ``profile_top_n``.

Full-path profiling
   Preserves more detailed path-level artifacts for deeper investigation.

Runtime Options
---------------

.. list-table::
   :header-rows: 1

   * - Option
     - Meaning
   * - ``-profile_metrics``
     - enable profiling collection
   * - ``-profile_top_n <N>``
     - keep the hottest ``N`` paths per category
   * - ``-profile_paths_full``
     - enable fuller path-level forensic output
   * - ``-profile_snapshot_interval_s <seconds>``
     - write periodic snapshots while Copper is still running

Output Layout
-------------

With profiling enabled, the job output directory typically contains:

- ``profiling/final/``
  - final per-rank profiling summaries and CSV files
- ``profiling/cluster/``
  - aggregated cluster-level summaries produced by
    ``scripts/aggregate_profiling.py``
- ``tables/final/``
  - raw cache or table outputs associated with the run

Common Files
------------

.. list-table::
   :header-rows: 1

   * - File
     - Meaning
   * - ``profiling_summary.md``
     - human-readable per-rank profiling summary
   * - ``profiling_operations.csv``
     - per-operation counts and timing
   * - ``profiling_aggregate.csv``
     - aggregate counters and cache metrics
   * - ``profiling_top_paths.csv``
     - hottest path list when top-path profiling is enabled

Key Terms
---------

``cache hit``
   Copper served the request from a local or forwarded cache path without
   repeating the underlying storage operation.

``Metadata ENOENT TTL store``
   Copper inserted an exact-path negative metadata result into the temporary
   ENOENT TTL cache.

``Metadata ENOENT TTL serve``
   Copper reused that temporary missing-path result instead of repeating the
   backend metadata lookup.

``Metadata ENOENT TTL expire``
   An ENOENT TTL entry aged out and was removed after its configured lifetime.

Operational Guidance
--------------------

- Use aggregate profiling first for scalable runs.
- Add top-path profiling when path hot spots matter.
- Reserve full-path profiling for smaller or targeted forensic runs.
- Do not rely on normal log verbosity to infer whether profiling ran; rely on
  the profiling output files themselves.
