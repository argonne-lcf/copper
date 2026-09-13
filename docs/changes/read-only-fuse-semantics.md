# Truthful read-only FUSE semantics

## Problem
Mutation and unsupported callbacks reported success, misleading applications.

## Design
Mutations return `-EROFS`; harmless lifecycle callbacks succeed; optional unsupported calls return `-ENOSYS`. Read-only open/access, backing-filesystem `statfs`, and bounded `lseek` are explicit policy helpers.

## Tests
`test_fuse_policy` exercises policy without mounting FUSE.

## Limitations
`SEEK_DATA`/`SEEK_HOLE` are not emulated and return unsupported/invalid results.
