Profiling and Metrics
=====================

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

Questions Profiling Answers
---------------------------

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

- :doc:`environment_path_analysis`
- :doc:`metadata_enoent_ttl_evaluation`
- :doc:`registration_and_scaling`
