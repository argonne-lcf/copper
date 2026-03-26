Overview and Best Practices
===========================

Purpose
-------

Copper reduces redundant metadata and data traffic during startup-heavy
workloads by using a read-only cooperative cache layered over FUSE and an RPC
overlay tree.

The intended production focus is:

- Python and shared-library import acceleration
- repeated read-mostly startup patterns
- scalable deployment on large node counts

Supported Runtime Focus
-----------------------

The core distributed operations are summarized below.

.. list-table::
   :header-rows: 1

   * - Operation
     - Production role
   * - ``getattr``
     - primary metadata lookup path
   * - ``read``
     - primary data path
   * - ``readdir``
     - primary directory enumeration path
   * - ``open``, ``release``, ``opendir``
     - lightweight compatibility stubs for read-mostly workflows

Mutation-style operations such as ``mkdir``, ``unlink``, ``rename``, and
``create`` are not the production focus of this tree.

Best-Practice Guidance
----------------------

The following recommendations are operational guidance for common deployments.
They are intended to help minimize configuration mistakes and avoid routing
unnecessary paths through Copper.

#. Prepend Copper paths to environment variables only when there is a specific
   need for a path to flow through Copper.
#. Do not prepend Copper blindly to variables such as ``PYTHONPATH``,
   ``VIRTUAL_ENV``, ``CONDA_PREFIX``, ``CONDA_ROOT``, ``LD_LIBRARY_PATH``, and
   ``PATH`` all at once.
#. When using a Python virtual environment or custom package directory,
   prepending Copper only to the relevant ``PYTHONPATH`` entry is usually
   sufficient.
#. When using a personal Conda environment, prepending Copper only to the path
   passed to ``conda activate`` is usually sufficient.
#. Prepend Copper to ``PATH`` only when a binary under a virtual environment or
   Conda environment must itself be resolved through Copper.
#. If an application takes input-file arguments that should flow through
   Copper, prepend Copper only to those input paths rather than to unrelated
   environment variables.
#. If a specific shared library should flow through Copper, prepend Copper only
   to the relevant ``LD_LIBRARY_PATH`` entry instead of rewriting the entire
   library path.
#. Copper is read-only. Do not use Copper-prefixed paths for output files,
   checkpoints, scratch directories, or temporary files.
#. Keep Copper service cores out of the application CPU binding list. Common
   example bindings are ``48,49,50,51`` on Aurora and ``1,2`` on Frontier,
   with some deployments also using ``1,2,65,66``. Site policies can differ.
#. Review applications and packages for hard-coded paths that may bypass or
   misuse Copper.
#. The commonly supported filesystem operations today are ``init``, ``open``,
   ``read``, ``readdir``, ``readlink``, ``getattr``, ``ioctl``, and
   ``destroy``.
#. Operations such as ``write``, ``unlink``, ``rename``, ``mkdir``, ``rmdir``,
   ``symlink``, ``statfs``, ``fsync``, ``flush``, and ``mmap`` are not part of
   the current production target.
#. System-default modules and frameworks whose metadata is already baked into
   the operating-system image often do not benefit from Copper.

Operational Recommendations
---------------------------

- Use Copper for read-mostly workloads.
- Prefer the prepared-address-book workflow at scale.
- Use ``facility`` mode when the site address book is trusted and current.
- Use ``discover`` mode when a fresh allocation-derived address map is
  required.
- Keep normal logging compact and rely on the retained startup timing lines for
  routine observability.
- Use the platform-specific examples in ``examples/aurora_examples`` and
  ``examples/frontier_examples`` as the starting point for site job scripts.

Key Runtime Signals
-------------------

The main startup timing lines retained at normal visibility are:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

These two lines are the default production indicators for startup progress
without reintroducing heavy debug logging.
