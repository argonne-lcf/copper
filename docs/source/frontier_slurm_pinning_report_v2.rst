Frontier Slurm Pinning Report
=============================

This note summarizes the current Slurm CPU-pinning guidance used by the
Frontier Copper examples and launch wrappers.

Purpose
-------

The goal of the Frontier pinning layout is to:

- keep Copper service threads on a small reserved core set
- keep application ranks off those service cores
- make the launch behavior easy to verify from Slurm output

Recommended Copper Service Cores
--------------------------------

The current Frontier wrappers default to:

- ``1,2``

Some deployments may also use:

- ``1,2,65,66``

When changing the Copper service-core list, update the application CPU-binding
list as well so the workload does not overlap with Copper.

Recommended Application Binding
-------------------------------

The Frontier examples use:

- ``CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73``

This keeps the eight application ranks on a node away from the common Copper
service cores.

Launch Behavior
---------------

The Frontier Copper launcher starts one ``cu_fuse`` process per node and uses:

- ``srun --ntasks-per-node=1 --cpus-per-task=2 --cpu-bind=verbose,none``
- ``taskset -c <physcpubind>``

This lets Slurm place the helper task while ``taskset`` enforces the final
Copper service-core placement inside that task.

Verification
------------

The simplest checks are:

- confirm the launcher prints the configured ``physcpubind`` value
- confirm Slurm shows the application ``cpu-bind=MAP`` output for the expected
  rank layout
- confirm the Copper service cores are not present in the application binding
  map

Operational Guidance
--------------------

- Keep the Copper service-core list small and explicit.
- Keep the application binding list explicit on Frontier runs.
- Avoid sharing Copper service cores with the workload.
- Treat ``1,2`` as the primary Frontier default unless local site guidance
  requires a larger reserved set.

Related Pages
-------------

- :doc:`aurora_and_frontier`
- :doc:`overview_and_best_practices`
- :doc:`production_notes`
