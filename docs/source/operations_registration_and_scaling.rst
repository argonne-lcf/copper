Registration and Scaling
========================

Problem Statement
-----------------

Copper startup at scale has to address two linked issues:

- children attempting parent RPCs before the parent provider was callable
- too much repeated address-book processing during startup

Runtime Design
--------------

Copper keeps the following production behaviors:

- a prepared job-local address book before the full Copper launch
- runtime support for ``-prefiltered_address_book``
- cached parent-readiness checking after the first successful readiness probe
- compact retained timing logs instead of the deeper debug-phase startup logs

Readiness Semantics
-------------------

The readiness evaluation work established a few definitions that are important
for reading Copper startup logs correctly.

``provider registration completed after <us>``
   The local rank finished its own provider setup and considers itself ready.
   This is a local statement only. It does not guarantee that the parent or any
   other remote rank is already callable.

``parent readiness confirmed``
   A child rank successfully probed its parent and confirmed that the parent is
   ready to accept forwarded RPCs.

``parent not ready yet``
   The child reached the parent address, but the parent was not yet ready to
   serve the requested RPC path.

``thallium exception while probing parent readiness ... HG_NOENTRY``
   The readiness probe itself reached the remote address, but the expected RPC
   or provider entry was not yet available there.

``timed out waiting for parent readiness``
   The child exhausted the overall readiness-wait budget before the parent
   became callable.

Address-Book Preparation
------------------------

``facility``
   One rank filters the provided facility address book down to the current job
   allocation.

``discover``
   The helper ``list_cxi_hsn_thallium`` is run across the allocation. Copper
   preserves the raw output and derives the final hostname-to-endpoint mapping
   from the column selected by ``net_type``.

Retained Timing Signals
-----------------------

The production-visible startup timing signals are:

- ``provider registration completed after <us>``
- ``first successful parent rpc_... completed after <us> since provider startup``

These lines are intentionally compact so that large-scale runs keep useful
startup observability without restoring heavy phase-by-phase debug logging.

To retain these two lines in current builds, use at least ``-l 4``. They are
emitted through ``LOG(INFO)``, so ``-l 0`` through ``-l 3`` will not show
them.

Observed Scaling Behavior
-------------------------

The retained evaluation data shows two different views of startup behavior:

- an earlier debug-phase evaluation that was useful for isolating the
  readiness race and provider-startup skew
- a later stable startup path that uses prepared address-book generation and
  compact retained timing signals

The earlier debug-phase evaluation showed three distinct regimes.

.. list-table::
   :header-rows: 1

   * - Scale
     - Import result
     - Average provider registration time
     - Main readiness outcome
   * - ``2-64`` nodes
     - success
     - ``145 ms`` to ``2.15 s``
     - readiness handshake is sufficient and clean
   * - ``128`` nodes
     - success
     - about ``10.25 s``
     - handshake helps, but ``HG_NOENTRY`` still appears during readiness probing
   * - ``256`` nodes
     - failed in the original evaluation
     - about ``44.17 s``
     - startup skew dominates; readiness timeouts and many probe failures appear

The most important quantitative finding was the provider-registration growth:

- ``145.36 ms`` at ``2`` nodes
- ``154.31 ms`` at ``16`` nodes
- ``2.15 s`` at ``64`` nodes
- ``10.25 s`` at ``128`` nodes
- ``44.17 s`` at ``256`` nodes

This is why the readiness probe mattered but was not, by itself, the complete
solution. By 128 and 256 nodes, children were often correct to wait; the parent
really was not ready yet.

Current Stable High-Scale Reference
-----------------------------------

The most recent retained high-scale registration measurements should be read as
the current stable reference, not as part of the earlier debug-heavy
``iter1``/``iter2`` diagnosis.

.. list-table::
   :header-rows: 1

   * - Scale and run
     - Registration log coverage
     - Provider registration min / avg / max
     - Main meaning
   * - ``256`` nodes latest retained high-scale reference
     - ``256 / 256`` ranks
     - ``149686 / 41917765.86 / 43527063 us``
     - startup is still expensive at this scale, but the run completed and the
       provider timing is fully captured
   * - ``512`` nodes current stable path
     - ``512 / 512`` ranks
     - ``140204 / 148274.60 / 171363 us``
     - prepared address-book startup removed the earlier high-scale
       registration blow-up and brought provider registration back down to
       about ``0.15 s``

These later results are the preferred reference when discussing current
production expectations at high scale.

Iter1 Versus Iter2 at High Scale
--------------------------------

This comparison should be read as a debugging-phase study, not as the final
production baseline. These runs were collected while the registration/readiness
logic was still under active investigation, with much heavier logging and with
code that did not yet match the later stable startup path.

The later ``iter2`` runs changed the probe policy from a fixed ``10 ms`` loop
with a ``5 s`` budget to an exponential-backoff policy with longer patience.

.. list-table::
   :header-rows: 1

   * - Scale and run
     - Import result
     - Import elapsed
     - Average provider registration
     - Readiness-probe exceptions
     - Main conclusion
   * - ``128`` nodes ``iter1``
     - success
     - ``1:07.83``
     - ``10.25 s``
     - ``1399``
     - handshake helped, but the original probe loop still saw many readiness failures
   * - ``128`` nodes ``iter2``
     - success
     - ``0:58.03``
     - ``10.44 s``
     - ``0`` in the aggregate pass
     - better probing removed the visible readiness-race symptoms
   * - ``256`` nodes ``iter1``
     - failed
     - ``0:54.23``
     - ``44.17 s``
     - ``16405``
     - provider-registration skew was too large for the original loop
   * - ``256`` nodes ``iter2``
     - success
     - ``1:45.52``
     - ``45.72 s``
     - ``843``
     - better probing improved correctness, but did not make provider startup faster
   * - ``512`` nodes ``iter1``
     - timed import completed, but cleanup was unstable
     - ``4:43.08``
     - one sampled rank: ``206.28 s``
     - still visible
     - provider-registration skew remained the dominant large-scale bottleneck

The table remains valuable because it shows why the readiness handshake and the
later startup redesign were needed. It should not, however, be used as the
best summary of the current stable code path.

Step-by-Step Runtime Sequence
-----------------------------

The readiness change altered the runtime sequence from:

``child immediately sends parent RPC``

to:

``child probes parent readiness, waits if needed, and only then sends the real parent RPC``

Operationally, the sequence is:

#. each rank starts its Mercury, Margo, and Thallium engine state
#. each rank performs local provider setup
#. the rank logs ``provider registration completed after <us>`` when its own
   provider is ready
#. a child rank later discovers that it needs parent service
#. the child sends a readiness probe first
#. if the parent is not ready, the child waits and retries
#. after ``parent readiness confirmed``, the child sends the real forwarded RPC

What the Code Measurement Means
-------------------------------

The code-path notes established a second important distinction:

- provider-registration timing measures local provider startup only
- it does not measure network-wide readiness

This explains why a rank can log ``provider registration completed`` and a
different child can still see ``HG_NOENTRY`` while probing that parent. Local
provider readiness and remote reachability are related, but they are not the
same event.

For practical startup measurement at scale:

- ``provider registration completed after <us>`` is the local registration
  completion time
- ``first successful parent rpc_... completed after <us> since provider startup``
  is the compact service-readiness time for the parent-connected distributed
  path

Operational Interpretation
--------------------------

The maintained conclusion from the registration-readiness work is:

- the readiness handshake is correct and useful
- better probe timing is also useful
- the long-term scaling bottleneck is still provider-registration skew

In other words, the dominant large-scale problem is no longer just a narrow
"child forwarded too early once" race. The larger issue is that provider
startup becomes too slow and too uneven across ranks.

Operational Use
---------------

- Use ``facility`` mode when the site-maintained address book is reliable.
- Use ``discover`` mode when endpoint discovery should be derived from the live
  allocation.
- Preserve ``logs/copper_address_book_full_output.txt`` when ``discover`` mode
  is used; it is the primary provenance artifact for discovered endpoint data.
- Treat ``provider registration completed after <us>`` and the first successful
  parent-RPC timing as the two key retained startup indicators at production
  verbosity.
