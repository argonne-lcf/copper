Production Notes
================

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
