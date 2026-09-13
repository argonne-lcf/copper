# Complete FUSE callback metrics

Every registered compatibility, read-only, optional, buffer, and seek callback now passes through the common completion metric wrapper. `read_buf` remains null so FUSE falls back to the instrumented `read`; `write_buf` is explicitly registered. Init, destroy, and the existing data callbacks retain their specialized timing paths.

Recursive-path rejection in `getattr`, `read`, `readdir`, `ioctl`, and `readlink` also completes the operation after timing starts. Each rejection records the negative outcome, `ENOENT`, elapsed latency, path, and the callback's metadata/data/tree/bypass profiler source instead of returning directly from a macro.
