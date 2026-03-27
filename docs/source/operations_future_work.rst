Future Work
===========

Priority Areas
--------------

#. Increase the FUSE ``max_read`` kernel setting where supported in order to
   improve large-read behavior.
#. Add a runtime flag or mode selection for RDMA-based data movement in
   addition to the current RPC-oriented path.
#. Improve multi-NIC support. Current production use typically assumes a
   single active NIC per node, even when the application can use all available
   NICs. The current Copper interception FUSE path is effectively limited to
   one NIC and may still spawn up to about ten child threads.
#. Continue evaluating threads, core placement, and execution-stream settings
   for ``cu_fuse`` and the ``-es`` runtime path.
#. Address serialization bottlenecks in the tree structure. Performance can
   still be constrained by a single root, and that limitation may remain even
   when the topology uses two or more roots.
#. Improve support for operations beyond the current read-mostly focus.
#. Revisit ``open``, ``release``, and ``opendir`` so they can move beyond
   lightweight compatibility stubs where required.
#. Expand support for mutation-oriented operations such as ``write``,
   ``unlink``, ``rename``, ``mkdir``, ``rmdir``, ``symlink``, ``statfs``,
   ``fsync``, ``flush``, and ``mmap``.
#. Strengthen failure detection and recovery, including restart handling and
   degraded-execution scenarios.
#. Add PBS integration for launch, staging, and operational workflows.
#. Integrate with higher-level workflow tools used in production pipelines.
#. Continue refining platform-specific examples and operational automation for
   both Aurora and Frontier.
#. Expand comparative evaluation against related approaches and tools,
   including PALS, PMIx in MPICH, Spindle, ``sbcast``, ``pbcast``,
   ``mpibcast``, VAST, DAOS, and tarball-shipping approaches that stage data
   in DRAM.
#. Run broader multi-tree experiments to understand scaling limits and root
   placement tradeoffs.
#. Evaluate machine-learning workloads such as ResNet to measure behavior
   outside the current benchmark set.

Current Scope Reminder
----------------------

The current production focus remains:

- ``getattr``
- ``read``
- ``readdir``

Operations such as ``write``, ``unlink``, ``rename``, ``mkdir``, ``rmdir``,
``symlink``, ``statfs``, ``fsync``, ``flush``, and ``mmap`` are not part of
the main production target for this tree.
