Profiling Overview
==================

Summary
-------

Copper profiling is intentionally separate from normal runtime logging and from
startup or address-book preparation behavior. Profiling can remain enabled even
when normal runtime logs are kept quiet.

Profiling Controls
------------------

Copper profiling is controlled by:

- ``-profile_metrics``
- ``-profile_top_n <N>``
- ``-profile_paths_full``
- ``-profile_snapshot_interval_s <seconds>``

Wrapper equivalents:

- ``-P``
- ``-N <N>``
- ``-A``
- ``-I <seconds>``

Validation Scope
----------------

The maintained profiling guidance is grounded in the version4 validation runs
that exercised a real ``python3 -c "import torch"`` workload on a small
multi-node allocation. Those runs were used to confirm four practical points:

- profiling files are emitted reliably when profiling is enabled
- aggregate, top-path, and full-path profiling can be enabled together
- ``pre-destroy`` snapshots preserve useful profiling information before final
  teardown
- the resulting outputs are useful both for performance interpretation and for
  path-level diagnosis

Representative launch settings from that evaluation included:

- metadata ``ENOENT`` TTL enabled at ``1000 ms``
- profiling enabled
- top-path limit set to ``20``
- full-path outputs enabled

Profiling Use Cases
-------------------

The profiling stack is intended to answer practical workload questions such as:

- which operations dominate request count
- which operations dominate elapsed time
- which paths dominate metadata traffic
- which paths dominate data traffic
- how much reuse is coming from Copper caches
- how much reuse is coming from the ENOENT TTL path

Validated Profiling Outputs
---------------------------

The profiling evaluation work validated three output modes on a real
``python3 -c "import torch"`` workload:

- aggregate summaries
- bounded top-path summaries
- full-path forensic outputs

The same evaluation also confirmed that the ``pre-destroy`` snapshot path is
working and that its outputs matched the final outputs closely in a clean run.

Representative findings from the 2-node validation were:

- all profiling output families were written successfully
- ``getattr`` dominated total call count
- ``read`` dominated total measured latency
- the profiler surfaced the repeated ``ENOENT`` probe paths that motivated the
  metadata TTL work
- the metadata TTL was active and useful, with ``1,131`` stores and
  ``9,715`` serves in the validation run

Representative Iter3 Cluster Totals
-----------------------------------

The maintained version4 dataset under ``docs/source/iter3`` adds a more
complete cluster summary for the same general ``import torch`` workload shape.
Across the two-rank cluster summary:

.. list-table::
   :header-rows: 1

   * - Metric
     - Value
   * - Total counted FUSE operations
     - ``3,968,662``
   * - Total cache hits
     - ``3,795,687``
   * - Total cache misses
     - ``2,284``
   * - Total negative results
     - ``68,095``
   * - Metadata ``ENOENT`` TTL stores
     - ``1,132``
   * - Metadata ``ENOENT`` TTL serves
     - ``9,715``
   * - Metadata ``ENOENT`` TTL expires
     - ``6``
   * - Total measured latency
     - ``50.351050 s``

In that same run, the operation totals retained the same overall shape:

.. list-table::
   :header-rows: 1

   * - Operation
     - Total calls
     - Total latency
   * - ``getattr``
     - ``3,770,238``
     - ``13.383411 s``
   * - ``read``
     - ``93,524``
     - ``35.790869 s``
   * - ``readdir``
     - ``2,304``
     - ``0.249776 s``

This continues to support the same interpretation:

- metadata discovery dominates total request count
- shared-library and file-content reads dominate total measured latency
- the ENOENT TTL continues to avoid repeated negative metadata work at a
  non-trivial scale even in a small two-rank run

Produced Files
--------------

When profiling is enabled, each Copper rank can produce several layers of
output. The version4 evaluation confirmed the presence of the following file
families:

- ``*-profiling_summary.md``
- ``*-profiling_operations.csv``
- ``*-profiling_aggregate.csv``
- ``*-profiling_top_paths.csv``
- ``*-data_table_cache_event.output``
- ``*-tree_table_cache_event.output``
- ``*-md_table_cache_event.output``
- ``*-md_ttl_cache_event.output``

The same run also produced the corresponding ``pre-destroy`` variants:

- ``*-pre-destroy-profiling_summary.md``
- ``*-pre-destroy-profiling_operations.csv``
- ``*-pre-destroy-profiling_aggregate.csv``
- ``*-pre-destroy-profiling_top_paths.csv``
- ``*-pre-destroy-*.output``

These outputs serve distinct purposes:

- aggregate CSVs summarize cluster-wide workload shape
- top-path CSVs preserve the hottest paths without unbounded output growth
- full-path and table-event outputs support deeper forensic analysis
- ``pre-destroy`` outputs provide an earlier preserved checkpoint when final
  shutdown is noisy

Final Full-Path Profiling Findings
----------------------------------

The final full-path profiling experiment provides the maintained production
shape for a profiling-enabled import workload.

Across the four-rank cluster summary:

.. list-table::
   :header-rows: 1

   * - Metric
     - Value
   * - Total counted FUSE operations
     - ``8,003,438``
   * - Total cache hits
     - ``7,592,110``
   * - Total cache misses
     - ``4,401``
   * - Total negative results
     - ``201,735``
   * - Metadata ``ENOENT`` TTL stores
     - ``2,513``
   * - Metadata ``ENOENT`` TTL serves
     - ``19,732``
   * - Total measured latency
     - ``37.399760 s``

At the operation level, the same cluster summary showed:

- ``getattr`` dominated total call count and metadata work
- ``read`` dominated cumulative measured latency
- ``readdir`` was present but much smaller than the metadata and data hot paths

Cross-Rank Stability
--------------------

The four per-rank final summaries were consistent with each other. Across the
four ranks, the observed ranges were:

.. list-table::
   :header-rows: 1

   * - Metric
     - Range across ranks
   * - Total counted FUSE operations
     - ``2,000,144`` to ``2,002,372``
   * - Total cache hits
     - ``1,897,747`` to ``1,898,160``
   * - Total cache misses
     - ``948`` to ``1,165``
   * - Total negative results
     - ``49,623`` to ``52,204``
   * - Metadata ``ENOENT`` TTL stores
     - ``560`` to ``824``
   * - Metadata ``ENOENT`` TTL serves
     - ``4,576`` to ``5,708``
   * - Total measured latency
     - ``9.111560 s`` to ``9.475459 s``

The ``pre-destroy`` summaries matched those same ranges, which is important
operationally: if shutdown becomes noisy, the pre-destroy snapshot still
preserves the profiling signal.

What the Profiles Say About the Workload
----------------------------------------

The full-path analysis also makes the path structure visible:

- the environment root and its parent directories dominate total path events
- the Python stdlib and ``site-packages`` remain very hot
- the Torch Python package and native Torch libraries are active immediately
- repeated missing shared-library probes are common but are usually expected

The iter3 cluster path heuristics make that same structure more explicit. The
largest observed path classes were:

.. list-table::
   :header-rows: 1

   * - Path class
     - Total events
     - Example
   * - ``environment_prefix``
     - ``2,828,400``
     - environment-root parent traversal such as ``/lustre``
   * - ``python_stdlib``
     - ``197,232``
     - ``.../conda_env/lib/python3.12``
   * - ``torch_python_package``
     - ``188,608``
     - ``.../site-packages/torch``
   * - ``python_site_packages``
     - ``163,484``
     - ``.../lib/python3.12/site-packages``
   * - ``torch_native_library``
     - ``57,503``
     - ``.../torch/lib/libmagma.so``
   * - ``missing_shared_library_probe``
     - ``1,084``
     - ``.../torch/lib/libhsa-amd-aqlprofile64.so``

That breakdown is useful because it shows that the dominant profile is not a
random scatter of unrelated paths. It is concentrated in:

- the environment root and its parents
- Python stdlib and package-discovery directories
- the Torch package tree
- a relatively small set of native Torch and ROCm libraries
- a still smaller set of repeated probe-miss paths

Concrete Hot-Path Examples
--------------------------

The version4 top-path outputs are most useful when read as a focused hotspot
report rather than as an exhaustive dump.

Representative metadata hot paths included:

- ``/lustre`` with ``175,759`` metadata events
- ``/lustre/orion`` with ``167,959`` metadata events
- the active ``conda_env`` root with ``113,359`` metadata events
- ``.../lib/python3.12/site-packages/torch`` with ``62,032`` metadata events

Representative data hot paths included:

- ``.../torch/lib/libmagma.so`` with ``9,091`` events
- ``.../torch/lib/libMIOpen.so`` with ``3,098`` events
- ``.../torch/lib/libtorch_hip.so`` with ``2,519`` events
- ``.../torch/lib/librocsparse.so`` with ``1,869`` events
- ``.../torch/lib/libtorch_cpu.so`` with ``1,860`` events

Representative TTL hot paths included:

- ``.../torch/lib/libhsa-amd-aqlprofile64.so`` with ``256`` to ``257`` TTL
  events in the version4 runs
- ``.../lib/python312.zip`` with roughly ``74`` to ``77`` TTL events
- ``.../lib/glibc-hwcaps`` with roughly ``64`` to ``65`` TTL events
- ``.../pyvenv.cfg`` with ``16`` to ``17`` TTL events

These are high-signal outputs because they answer three different operational
questions directly:

- which directories dominate metadata traffic
- which shared libraries dominate read traffic
- which repeated misses are worth treating as environment-path cleanup targets

Operationally, those findings support two different profiling use cases:

- workload measurement, where the main question is which operations or path
  classes dominate total work
- environment diagnosis, where the main question is which duplicated, stale,
  or optional probes can be reduced without breaking the active runtime

Metric Reading Guide
--------------------

The version4 evaluation also clarified a set of terms that are worth keeping
close to the maintained profiling pages.

``Counted FUSE operations``
   The total number of requests observed by the profiler. This is the broadest
   measure of workload volume.

``Measured latency``
   Time recorded by the profiler while handling operations. This appears both
   as per-operation average latency and as cumulative total latency.

``Cache hit``
   A request answered from Copper's cache path without repeating the full
   underlying lookup or read path.

``Cache miss``
   A request that required additional work because Copper did not already hold
   a reusable cached answer.

``Negative result``
   A request that completed with a not-found or similar unsuccessful result.
   Many such results are expected during Python and dynamic-loader startup.

``Top-path profiling``
   A bounded hotspot view that preserves the busiest paths while avoiding the
   output growth of a complete per-path dump.

``Full-path profiling``
   A more forensic mode that retains wider path-level evidence and is best
   reserved for targeted runs.

``Pre-destroy snapshot``
   A profiling checkpoint written before final teardown so that useful
   measurements survive even when the shutdown path becomes noisy.

That is why the documentation now treats profiling as both:

- a performance-measurement tool
- a configuration-cleanup tool for environment paths

Startup Timing Versus Profiling
-------------------------------

The following timing lines are useful alongside profiling, but they are not
profiling outputs:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

Those lines describe startup/readiness behavior. Profiling files remain the
authoritative source for workload measurements.

Related Pages
-------------

- :doc:`operations_environment_path_analysis`
- :doc:`operations_metadata_enoent_ttl_evaluation`
- :doc:`operations_registration_and_scaling`
