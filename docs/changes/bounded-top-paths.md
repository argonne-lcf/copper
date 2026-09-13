# Bounded path profiling

## Problem
Top-path mode retained every path and only truncated while reporting, so memory was unbounded.

## Design
The production `ProfilerCore` bounds retained `(source,path)` records during request completion using an exact first-N admitted set plus an explicit `other_events` bucket. It exports `paths_seen`, `paths_retained`, `paths_dropped`, and `other_events`; retained rows never inherit another path's attribution. Full-path mode remains exact. This is intentionally not a frequency heavy-hitter algorithm.

## Tests
Unit tests verify bounded cardinality and exact full mode.

## Limitations
The deterministic bounded policy is conservative rather than a probabilistic frequency estimator.
