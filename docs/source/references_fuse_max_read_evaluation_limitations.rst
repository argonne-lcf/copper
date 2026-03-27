FUSE Max Read Evaluation Limitations
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

Related Reference
-----------------

The fuse-devel discussion `read more than 128kB
<https://fuse-devel.narkive.com/OiMjwv8y/read-more-than-128kb>`_ reinforces
the same conclusion. The thread shows that increasing ``max_read`` or even
raising ``FUSE_MAX_PAGES_PER_REQ`` does not, by itself, guarantee large
application-visible reads. In the reported case, the effective read size was
still limited by the runtime path in use, including kernel version, readahead
behavior, and whether buffered I/O or ``direct_io`` was enabled.

It also highlights an additional performance caveat: buffered FUSE reads can
incur extra memory copies, so throughput can be bottlenecked by copy overhead
even when the nominal read-size configuration looks larger. The practical
lesson is that observed request size and throughput must be validated end to
end; mount options such as ``max_read``, ``large_read``, and ``direct_io``
need to be evaluated together with kernel constraints and the actual workload.
