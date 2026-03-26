Metadata ENOENT TTL Evaluation
==============================

Scope
-----

This page integrates the maintained findings from the ``-E`` evaluation work
for exact-path metadata ``ENOENT`` caching. The core experiment measured how
``launch_copper.sh -E <value>`` affected:

- ``python3 -c "import torch"`` completion time
- repeated metadata rechecks
- backend ``ENOENT`` traffic
- total negative metadata churn

This dataset should be read as an older debugging-phase reference, not as the
final stable production baseline. The runs were collected while the code still
had heavier debug logging and before the later stable startup path was fully in
place. The main value of the dataset is as a rough estimate of TTL behavior on
the older code path.

Summary of the 128-Node Evaluation
----------------------------------

The 128-node experiment compared ``E100``, ``E1000``, ``E2000``, ``E5000``,
and ``E10000``.

This 128-node summary is best treated as a rough tuning reference for the
older code line. It remains useful for understanding the relative effect of
small versus moderate ``-E`` values, but it should not be read as the
authoritative performance ranking for the current stable code.

.. list-table::
   :header-rows: 1

   * - ``-E`` value
     - Import result
     - Elapsed
     - Stale completed entries
     - TTL stores
     - TTL serves
     - Backend ``ENOENT`` lookups
   * - ``100``
     - success
     - ``57.94 s``
     - ``15,023``
     - ``110,608``
     - ``632,028``
     - ``3,449``
   * - ``1000``
     - success
     - ``48.79 s``
     - ``1,511``
     - ``86,575``
     - ``642,517``
     - ``818``
   * - ``2000``
     - success
     - ``47.80 s``
     - ``1,017``
     - ``85,421``
     - ``643,206``
     - ``624``
   * - ``5000``
     - success
     - ``48.03 s``
     - ``1,020``
     - ``85,117``
     - ``643,482``
     - ``606``
   * - ``10000``
     - success
     - ``47.02 s``
     - ``872``
     - ``84,852``
     - ``643,556``
     - ``571``

Key Findings
------------

``E100`` is too small at 128 nodes
   It produced the slowest import time and the highest stale-recheck count.

Most of the gain arrives by ``E1000`` to ``E2000``
   The largest step improvement happened between ``E100`` and ``E1000``.

Larger values continue to help, but with diminishing returns
   ``E5000`` and ``E10000`` still reduce stale rechecks and backend
   ``ENOENT`` traffic, but only modestly compared with the ``E1000`` to
   ``E2000`` transition.

``E10000`` gave the best raw import time in this dataset
   The difference relative to ``E2000`` was narrow, so the best choice depends
   on whether absolute speed or operational conservatism matters more.

Recommended Defaults by Scale
-----------------------------

The archived evaluation and follow-on test planning support the following
practical starting points:

.. list-table::
   :header-rows: 1

   * - Scale range
     - Recommended starting point
     - Notes
   * - ``2-16`` nodes
     - ``E1000``
     - already large enough to collapse common repeated misses
   * - ``128-512`` nodes
     - ``E2000``
     - best overall balance in the 128-node dataset
   * - ``1K-10K`` nodes
     - compare ``E2000`` and ``E5000``
     - carry both until the larger-scale workload is characterized

Operational Guidance
--------------------

- keep ``E100`` out of larger-scale production comparisons
- use ``E2000`` as the default balanced starting point unless the workload
  shows a clear benefit from larger settings
- keep observing ``stale completed entry``, TTL serves, and backend
  ``ENOENT`` counts together
- remember that the TTL reduces repeated work; it does not eliminate the
  workload's tendency to ask for missing paths
- treat the 128-node numbers on this page as directional guidance from an
  older debug-era evaluation rather than as the final word on current stable
  behavior

What the TTL Fix Does Not Solve
-------------------------------

The metadata ``ENOENT`` TTL is useful, but narrow. It does not by itself solve:

- a workload that keeps asking for many distinct missing paths
- heavy directory enumeration churn
- read-path amplification for file content
- transport or RPC failures unrelated to ``ENOENT``
- mount instability after upstream Copper failures

The right interpretation is that the TTL is a targeted reduction in repeated
negative metadata work, not a general cure for all startup pathologies.
