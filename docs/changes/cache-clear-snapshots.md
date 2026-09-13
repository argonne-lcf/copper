# Cache clear and synchronized snapshots

## Problem
The clear IOCTL cleared metadata twice, omitted data, and cache reporting traversed maps without locks.

## Design
Cache owns `clear`, `size`, and copy-snapshot methods. Clear paths use those methods and cover tree, metadata, and data caches. Reports sort copied snapshots.

## Tests
`test_cache` verifies snapshot independence, clear behavior, and concurrent insertion.

## Limitations
Cache lookups return owned values, so snapshots and values already returned to callers remain valid across a concurrent clear. This does not make compound read-modify-write sequences atomic.
