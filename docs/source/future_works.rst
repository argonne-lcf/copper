Future Work
===========

Priority Areas
--------------

#. Increase the FUSE ``max_read`` kernel setting where supported in order to
   improve large-read behavior.
#. Add a runtime flag or mode selection for RDMA-based data movement in
   addition to the current RPC-oriented path.
#. Improve support for multiple NICs per node. Current production use typically
   assumes a single active NIC per node.
#. Continue evaluating threads, core placement, and execution-stream settings
   for ``cu_fuse`` and the ``-es`` runtime path.
#. Improve support for operations beyond the current read-mostly focus.
#. Revisit ``open``, ``release``, and ``opendir`` so they can move beyond
   lightweight compatibility stubs where required.
#. Expand support for mutation-oriented operations such as ``write``,
   ``unlink``, ``rename``, ``mkdir``, ``rmdir``, ``symlink``, ``statfs``,
   ``fsync``, ``flush``, and ``mmap``.
#. Continue refining platform-specific examples and operational automation for
   both Aurora and Frontier.

Current Scope Reminder
----------------------

The current production focus remains:

- ``getattr``
- ``read``
- ``readdir``

Operations such as ``write``, ``unlink``, ``rename``, ``mkdir``, ``rmdir``,
``symlink``, ``statfs``, ``fsync``, ``flush``, and ``mmap`` are not part of
the main production target for this tree.
