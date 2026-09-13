# Negative failure statuses

Read, readiness, and parent-RPC failures now use signed negative errno values (`-EIO` when no more specific errno is available), rather than negating the unsigned `fs_operation_error` sentinel. The shared signed-status predicate ensures these failures cannot enter the data cache as successful reads.

IOCTL report-stream open failures preserve the positive `errno` exposed by the stream helper, convert it to a negative FUSE status (falling back to `-EIO`), and pass that exact status through `Metric::stop_cache_operation` as a negative bypass outcome.

Dependency-light compiled tests cover local/readiness/RPC failure status and cache admission; production wiring tests cover IOCTL status/metric routing.