Copper Documentation
====================

Copper is a read-only cooperative caching layer for scalable metadata and data
reuse on large HPC systems. This documentation set is organized for production
deployment, runtime operations, profiling, scaling behavior, and engineering
reference material.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   overview_and_best_practices
   aurora_and_frontier
   building_and_running
   building_and_platforms

.. toctree::
   :maxdepth: 2
   :caption: Runtime And Operations

   profiling_and_metrics
   profiling_metrics
   environment_path_analysis
   metadata_enoent_ttl_evaluation
   registration_and_scaling
   production_notes
   future_works

.. toctree::
   :maxdepth: 2
   :caption: Engineering Notes And Reports

   deep_debugging
   frontier_slurm_pinning_report_v2
   copper_false_enoent_report
   fuse_max_read_technical_report
   fuse_max_read_evaluation_limitation
   hsn_cxi_address_calculation_report
   evaluation_reports_index
