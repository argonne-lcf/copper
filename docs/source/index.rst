Copper Documentation
====================

Copper is a read-only cooperative caching layer for scalable metadata and data
reuse on large HPC systems. This documentation set is organized as a practical
manual: start with the overview, move into build and platform guidance, then
use the profiling and runtime behavior sections to study launches, scaling,
and path usage.

.. toctree::
   :maxdepth: 2
   :caption: Overview

   guide_overview_and_best_practices

.. toctree::
   :maxdepth: 2
   :caption: Platforms and Build

   guide_build_and_run
   guide_build_artifacts_and_platforms
   guide_aurora_and_frontier

.. toctree::
   :maxdepth: 2
   :caption: Running and Analysis

   operations_launch_and_analysis_runbook
   operations_profiling_overview
   operations_profiling_reference
   operations_environment_path_analysis

.. toctree::
   :maxdepth: 2
   :caption: Runtime Behavior and Scaling

   operations_metadata_enoent_ttl_evaluation
   operations_registration_and_scaling
   operations_production_notes
   operations_future_work

.. toctree::
   :maxdepth: 2
   :caption: Reference Notes

   references_deep_debugging
   references_frontier_slurm_pinning
   references_false_enoent_report
   references_fuse_max_read_technical_report
   references_fuse_max_read_evaluation_limitations
   references_hsn_cxi_address_calculation
   references_evaluation_reports_index
   references_sample_outputs
