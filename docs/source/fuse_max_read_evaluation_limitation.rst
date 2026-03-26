FUSE Max-Read Evaluation Limitations
====================================

This note records the main limitations of the current FUSE ``max_read``
evaluation work.

Current Limits
--------------

- changing userspace ``max_read`` alone does not guarantee larger effective
  reads
- kernel page limits can still cap request size
- cached-mode behavior can obscure the direct effect of a userspace change
- application-visible throughput can improve or regress for reasons unrelated
  to the nominal configured ``max_read`` value

Recommended Next Step
---------------------

Treat FUSE ``max_read`` as a coordinated tuning problem rather than a single
knob. Any future implementation work should measure:

- negotiated userspace values
- kernel-side limits
- effective request sizes observed in practice
- end-to-end throughput for representative workloads
