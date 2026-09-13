# Atomic metadata TTL profiling state

Metadata ENOENT TTL store, serve, expire, and clear totals and per-path attribution now live inside `ProfilerCore`. Operation, path, and TTL state are copied or reset under the same mutex, so a report/reset cannot combine different epochs. Bundle aggregate and path CSVs include TTL counters.

The TTL cache itself remains independently synchronized because its contents are filesystem behavior, not profiler epoch state.
