# Coherent profiler core

## Problem
Wall-clock timing, separately updated counters, and live report reads could produce incoherent observations.

## Design
The production `Metric::stop_*` path now performs one `Operations::complete` call into `ProfilerCore`. `Operations`, `CacheEvent`, and `Profiling` are views/facades over that collector, and each report bundle is rendered from one immutable snapshot. Snapshot/reset swaps raw state under the collector mutex and computes quantiles/sorting after releasing it. The periodic worker uses an interruptible condition variable.

Collection is runtime-gated by `-profile_metrics`, `-profile_top_n`, or `-profile_paths_full`; with all three disabled, completion returns before clock/collector work and report generation returns `profiling disabled` without creating output. Legacy operation/cache metric views reflect the same gated `ProfilerCore` and are therefore not always-on.

## Tests
`test_profiler_core` checks snapshot invariants, reset, concurrency, outcomes, errors, and generated schema. `test_production_profiler_wiring.py` fails if the runtime target or `Metric`/`Profiling` path disconnects the core.

## Limitations
Legacy one-metric IOCTL views remain compatibility views and each obtains its own snapshot; only production profiling bundles promise cross-field snapshot coherence.
