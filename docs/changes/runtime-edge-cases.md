# Runtime edge-case hardening

## Problem
Large reads ignored open failure and could lose `pread` errno; zero-size readlink underflowed; hostname termination and full readdir buffers were unsafe; post-RPC cache assertions aborted.

## Design
Preserve syscall errors, handle close failures, reject zero-size readlink, force hostname termination, stop cleanly when the FUSE buffer fills, and return `EIO` for missing post-RPC entries.

## Tests
Policy tests cover boundary behavior; compilation checks touched callbacks.

## Limitations
Full mounted-FUSE integration requires the cluster dependency environment.
