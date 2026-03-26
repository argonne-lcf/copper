Evaluation Reports Index
========================

This page summarizes the archived evaluation work that informed the maintained
documentation set. The goal here is to retain the substantive findings in
production documentation tone instead of pointing readers only to raw report
paths.

Profiling Evaluations
---------------------

The profiling reports established that Copper profiling is operationally useful
at three levels:

- aggregate summaries for high-level workload shape
- top-path outputs for bounded hotspot analysis
- full-path outputs for forensic environment and library-path diagnosis

The version4 profiling evaluation also confirmed that the ``pre-destroy``
snapshot path preserves the same essential signal as the final outputs, which
is valuable when shutdown becomes noisy.

The maintained summaries now live in:

- :doc:`profiling_and_metrics`
- :doc:`environment_path_analysis`

Registration and Scaling Evaluations
------------------------------------

The registration-readiness reports showed that the original narrow race was
real, but that the larger scaling bottleneck was provider-registration skew.

The maintained conclusions are:

- local provider registration is not the same as remote parent readiness
- the readiness handshake is correct and worth keeping
- improved probing policy reduces visible race symptoms
- provider-registration skew is the dominant remaining large-scale bottleneck

Those findings are now integrated into:

- :doc:`registration_and_scaling`

Metadata ``ENOENT`` TTL Evaluation
----------------------------------

The ``-E`` evaluation established that:

- very small TTL windows such as ``E100`` are too small at 128 nodes
- most of the gain arrives by roughly ``E1000`` to ``E2000``
- ``E2000`` is the strongest balanced default from the measured dataset
- ``E10000`` gave the best raw time in the 128-node test, but only narrowly

That evaluation should be read as an older debug-phase reference. It was
collected before the later stable startup path was in place, so it is most
useful as a rough estimate and tuning guide for the older code line rather
than as the final production baseline.

Those findings are now integrated into:

- :doc:`metadata_enoent_ttl_evaluation`

How to Use the Maintained Pages
-------------------------------

Use the maintained pages under ``docs/source`` for current operational
guidance. The archived experiment directories under ``exp_results`` remain
useful as source material, but the production documentation should be the first
reading path for most users.
