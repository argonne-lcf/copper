Profiling Reference
===================

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

``FUSE operation``
   A filesystem request issued through the Copper mount, such as ``getattr``,
   ``read``, ``readdir``, or ``open``.

``Metadata``
   File information rather than file content, including existence, type, size,
   and permissions.

``Data``
   File content bytes read from a file, such as the bytes of a shared library
   or Python module.

``Negative result``
   A request that ends in a missing-path or similar unsuccessful result rather
   than a successful metadata or data response.

``ENOENT``
   The standard Unix "No such file or directory" result. In profiling output,
   repeated ``ENOENT`` paths are often informative rather than erroneous.

``Top-path profiling``
   A bounded hotspot view that retains the busiest paths instead of dumping the
   full path population.

``Full-path profiling``
   A deeper forensic mode that preserves more complete per-path evidence and
   can become large at scale.

``Pre-destroy snapshot``
   A profiling snapshot written before the final Copper teardown sequence.

Reading the Metrics
-------------------

The maintained profiling evaluations support the following interpretations:

- high ``getattr`` counts usually indicate metadata-heavy startup or import
  behavior
- high cumulative ``read`` latency usually indicates that content reads, not
  metadata probes, dominate elapsed service time
- high ``Metadata ENOENT TTL serve`` counts are generally a positive sign that
  repeated negative metadata probes are being collapsed successfully
- repeated missing shared-library probes, ``python*.zip`` checks, and
  ``pyvenv.cfg`` lookups are often expected runtime behavior rather than
  correctness bugs

Validated Output Families
-------------------------

The version4 profiling validation confirmed that a profiling-enabled run can
produce:

- per-rank Markdown summaries
- per-rank operation CSVs
- aggregate CSVs
- bounded top-path CSVs
- raw cache-event outputs
- matching ``pre-destroy`` variants of those artifacts

That output structure is what makes profiling useful both for quick workload
inspection and for deeper environment-path debugging.

Operational Guidance
--------------------

- Use aggregate profiling first for scalable runs.
- Add top-path profiling when path hot spots matter.
- Reserve full-path profiling for smaller or targeted forensic runs.
- Do not rely on normal log verbosity to infer whether profiling ran; rely on
  the profiling output files themselves.
