# Bounded latency distributions

## Problem
Each operation retains exact epoch count, cumulative latency, minimum, and maximum. Quantiles are estimates computed from a bounded Algorithm R reservoir. Reports explicitly expose `sample_count` and `sampling_method=reservoir_algorithm_r`; after count exceeds capacity, quantile samples are not the full epoch.

## Design
Production request completions retain bounded samples and generated operation reports expose count, min, p50, p90, p95, p99, p999, max, and cumulative nanoseconds. Timing uses `steady_clock`; quantile sorting occurs after snapshot capture, outside the collection mutex.

`COPPER_PROFILE_LATENCY_SAMPLES` accepts only an unsigned decimal integer from `0` through `65536`, inclusive. `0` disables quantile sample retention. Unset, empty, negative, malformed, overflowing, or above-maximum values use the existing default capacity of `1024`; the bound prevents accidental impractical allocations.

## Tests
Unit tests use a five-value unsampled epoch and assert the implemented nearest-rank indices exactly: p50=30 and p90/p95/p99/p999=50. They separately assert exact cumulative/min/max values while reservoir sampling is active and cover zero, negative, malformed, overflow, maximum, and maximum-plus-one configuration values.

## Limitations
Bounded sampling gives approximate quantiles after capacity is exceeded.
