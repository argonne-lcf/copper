# Fail-closed Aurora launcher

## Problem and impact

Aurora `clush` ranks start in non-login remote shells, so a custom Copper build could not see the caller's runtime `PATH`, `LD_LIBRARY_PATH`, or libfabric/Mochi settings. In particular, `cu_fuse` could fail to load `libmargo.so`. The launcher ignored the failing `clush` status, slept, and returned success without checking that mounts existed.

## Behavior and design

`launch_copper_aurora.sh` now:

- serializes a strict allowlist of runtime variables (`PATH`, `LD_LIBRARY_PATH`, `COPPER_ROOT`, and libfabric/Mochi-related prefixes) with Bash `%q` quoting before passing them to remote `env`;
- constructs Copper arguments as an array and shell-quotes every remote argument, including paths containing spaces or shell metacharacters;
- fails immediately if remote directory preparation or any `clush` Copper startup rank fails; and
- waits for the configured startup interval, then uses `mountpoint`, `findmnt`, or `/proc/mounts` on every host and fails unless every mount is active.

The initial best-effort unmount remains non-fatal because no prior mount is a normal launch state. Stop behavior is unchanged: it is intentionally best-effort cleanup, while launch now owns fail-closed readiness reporting.

## Tests and verification

`tests/test_launch_copper_aurora.py` uses temporary fake `clush`, `cu_fuse`, `fusermount3`, and `mountpoint` commands. It removes the inherited environment in the fake remote shell and verifies:

1. required runtime values survive safely, including spaces, semicolons, and command-substitution text;
2. a rank startup failure produces a nonzero launcher result; and
3. a successful rank command without an active mount still produces a nonzero result.

The test is registered with dependency-light CTest when Python is available. Local verification covers shell syntax and simulated remote behavior; no new mounted Aurora hardware validation is claimed.
