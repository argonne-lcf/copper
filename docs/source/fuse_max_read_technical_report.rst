FUSE Max-Read Technical Report
==============================

Problem Statement
-----------------

Copper uses FUSE as its filesystem front-end, so effective read size directly
affects per-request overhead, context switching, and cache efficiency for
read-heavy workloads.

Main Finding
------------

FUSE read performance is not controlled by a single parameter. The effective
read size observed by an application is shaped by:

- libfuse negotiation through ``conn->max_read``
- mount-time ``max_read`` handling
- Linux kernel request-size limits such as page-based caps
- cached-mode readahead behavior

Implication
-----------

Increasing read throughput requires coordinated tuning across userspace,
kernel negotiation, and cache behavior. Changing only one ``max_read`` value is
not sufficient for a reliable improvement.

Production Guidance
-------------------

The FUSE ``max_read`` topic remains a technical optimization area rather than
an integrated production feature. Any future work in this area should be
validated against:

- libfuse negotiation behavior
- kernel page limits
- cached-mode and readahead interactions
- observed application-side request sizes
