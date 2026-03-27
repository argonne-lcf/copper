Runtime Tuning and Notes
========================

Intended Defaults
-----------------

The intended production posture is:

- keep profiling available
- keep the startup/readiness/address-book scaling fixes enabled
- keep normal logging compact

Logs to Keep Visible
--------------------

These signals should remain visible at normal operational verbosity:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``
- warnings, errors, and readiness timeouts

Logging Level Required for Startup Timing
-----------------------------------------

These retained startup timing lines are emitted through ``LOG(INFO)`` in the
current Copper source. That means they are not visible at ``-l 0``.

To see:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

use at least:

- ``-l 4``

In the current log-level mapping:

- ``-l 0`` disables logging
- ``-l 1`` through ``-l 3`` do not retain ``INFO`` lines
- ``-l 4`` is the first level that includes ``INFO``
- ``-l 5`` enables the more debug-heavy output

For scale tests that only need startup timing and readiness visibility without
restoring the full debug-phase chatter, ``-l 4`` is the intended setting.

Logging Levels at a Glance
--------------------------

Copper uses a user-facing ``-l`` / ``-log_level`` setting to control how much
of the internal logger is retained.

.. list-table::
   :header-rows: 1

   * - User level
     - Meaning
     - Typical use
     - Example visibility
   * - ``-l 0``
     - no logging
     - near-silent production run
     - suppresses retained startup timing lines
   * - ``-l 1``
     - fatal only
     - crash-only visibility
     - only severe termination-level failures
   * - ``-l 2``
     - error and fatal
     - error-focused troubleshooting
     - startup failures, hard RPC failures, address-book errors
   * - ``-l 3``
     - warning, error, and fatal
     - non-fatal issue diagnosis
     - warnings, errors, readiness timeouts
   * - ``-l 4``
     - info, warning, error, and fatal
     - startup timing and scale studies
     - ``provider registration completed after ...`` and
       ``first successful parent rpc_... completed after ...``
   * - ``-l 5``
     - debug-heavy / most logging
     - deep runtime debugging
     - debug chatter, repeated coordination details, cache-path details

For practical use:

- use ``-l 4`` when the goal is to measure Copper startup and readiness timing
- use ``-l 5`` when investigating a startup race, parent-readiness behavior,
  or detailed cache-path flow
- use ``-l 0`` when a production run should remain quiet and the timing lines
  are not needed

Example Startup Visibility
--------------------------

At ``-l 4``, the compact startup path is expected to retain lines such as:

- ``starting thallium engine``
- ``engine started``
- ``using network type: ...``
- ``server running at address: ...``
- ``provider registration completed after ...``
- ``first successful parent rpc_... completed after ...``

At ``-l 0``, those same lines are suppressed.

Operational Meaning of the Retained Timing Lines
------------------------------------------------

These two timing lines are the compact production signals for when Copper has
become usable at startup scale:

``provider registration completed after <us>``
   The local rank finished its own provider setup. This is the local
   registration-complete time for that rank.

``first successful parent rpc_... completed after <us> since provider startup``
   The rank completed its first successful parent-directed RPC after provider
   startup. This is the practical readiness signal that the rank has advanced
   beyond local startup and has successfully entered the parent-connected
   service path.

For startup studies, it is reasonable to treat the second line as the compact
``ready`` time for the distributed Copper service path.

Logs to Demote
--------------

Hot-path provider chatter should remain at debug-scale or higher-verbosity
levels rather than normal operational visibility.

Examples include:

- repeated requester-address logging
- repeated cache-hit chatter
- repeated path coordination chatter

Address-Book Provenance
-----------------------

When ``discover`` mode is used, retain both of the following artifacts:

- ``logs/copper_address_book.txt``
- ``logs/copper_address_book_full_output.txt``

The raw full-output file is the authoritative provenance record for what the
discovery helper observed on the allocation.

Conda Environment Note
----------------------

When a personal Conda environment is used through Copper, prepend
``/tmp/${USER}/copper/`` only to the path used with ``conda activate``.
