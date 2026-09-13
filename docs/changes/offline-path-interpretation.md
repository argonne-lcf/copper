# Offline path interpretation

## Problem
Runtime C++ embedded workload-specific path classification and pruning advice.

## Design
Runtime guidance was removed; `aggregate_profiling.py` owns classification and interpretation. Hostname matching now supports hyphenated hosts.

For schema-v2 top-path rows, any nonzero `ttl_stores`, `ttl_serves`, `ttl_expires`, or `ttl_clears` field identifies TTL data regardless of `source`. This is necessary because the runtime records TTL paths with metadata source provenance rather than a synthetic TTL source.

## Tests
Python unit tests cover representative classifications and exact schema-v2 TTL CSV emission/summary behavior.

## Limitations
Heuristics remain advisory and workload-specific in post-processing.
