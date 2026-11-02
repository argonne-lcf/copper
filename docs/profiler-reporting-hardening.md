# Offline profiler reporting

## Entry points

```sh
scripts/copper-report /path/to/log --expected-host host-a --expected-host host-b \
  --run-id run-123 --daemon-generation generation-1
scripts/copper-compare /baseline/profiling/cluster/cluster-current \
  /candidate/profiling/cluster/cluster-current
```

`aggregate_profiling.py` remains the backward-compatible reporting entry point.
An optional positional output prefix defaults to `cluster`; `--output-dir` changes
its output directory. Report inputs may be the bundle directory directly or a log
root containing `profiling/final`. No daemons, RPCs or mounted filesystem are needed.
Each command's `--help` is executable. Comparison exits 0 for compatible reports,
2 for explicit compatibility mismatches (JSON diagnostics, no misleading deltas),
and 1 for malformed/corrupt input. `--output-dir` on comparison publishes its JSON
through an atomic generation; otherwise JSON is written to stdout.

## Schema and exact integrity

Runtime schema 3 is consumed according to `profiler-runtime-hardening.md`, including
all four exact data basenames, raw-byte SHA256/size metadata, 65 inclusive integer
histogram buckets and path `estimated_total,error_bound` fields. Integrity is checked
before parsing and rechecked before bundle validation. Orphan CSV files are ignored;
malformed committed schema3, missing committed data, duplicate identities/shared
inodes, malformed CSV row shapes, inconsistent counters and corrupted data fail
closed. Integer counters remain integers; uint64 bounds and operation/histogram
aggregate overflow are rejected. Hashes detect corruption, not malicious rewriting
of manifests, and input bundles should remain immutable during report generation.

Legacy schema2 requires `--legacy-unverified`. Its reports never claim verified
complete and have no mergeable cluster percentiles. Unsupported older schemas and
malformed legacy manifests retain the legacy discovery behavior.

## Coverage, restarts and resets

An exact repeated `--expected-host` roster is mandatory. Unexpected hosts fail;
missing hosts need `--allow-partial` and are recorded. A schema3 selection must have
one nonnull run/generation identity. `--run-id` and `--daemon-generation` can select
one explicit identity when a directory contains multiple runs/generations. Process
sequence is not treated as a restart clock: multiple PIDs under one host/generation
fail rather than guessing. Select the desired restart generation explicitly; no
cross-generation snapshots are ever summed.

Within a host/PID/generation, sequences must be unique and reset epochs monotonic.
For each epoch the latest cumulative snapshot is used, or the single closing
interval replaces earlier cumulative snapshots. An interval followed by cumulative
of the same epoch or multiple intervals fails as overlap. Closed earlier epochs
plus the latest epoch are summed exactly once. Missing epochs or earlier epochs
without closure require `--allow-partial`, with explicit `epoch_gaps` and incomplete
coverage. Cumulative operation/errno counters and histogram buckets cannot regress
within an epoch. The latest snapshot per host must be `kind=final` for complete
coverage; earlier closed periodic intervals are valid. Unknown identities cannot
be made verified by enabling partial mode.

## Reports and comparison

One report inventory includes aggregate/operation CSV, Markdown, JSON and, for
schema3, merged histogram CSV. Cluster p50/p90/p95/p99/p999 are nearest-rank bucket
**lower and upper bounds**, computed with integer rank arithmetic after merging all
completion histogram counts. Reservoir percentiles are never averaged. Per-operation
host bounds and the distribution of host upper bounds (min, lower median, max)
are separately labeled: they are not cluster percentiles. Operations are ranked by
cumulative measured latency, not claimed wall-clock duration. Source counters,
errno breakdowns, host totals and host cost imbalance are preserved.

Retained path rows are kept separately by host/reset epoch and include source,
exact counters, estimates, error bounds and manifest provenance. Independently
tracked heavy-hitter estimates are never merged as exact counts. Legacy coarse
path-class/pruning hints remain observational, not deletion authorization.

JSON preserves selected manifests and hashes, source manifest metadata,
run/generation, coverage and compatibility parameters. Optional `--startup-receipt`,
`--workload-receipt`, `--cleanup-receipt`, `--cache-receipt` join supplied JSON evidence
only when its `run_id` matches. Missing, unreadable or mismatched evidence is
explicitly unavailable/mismatched, never invented. Existing legacy root coverage
analysis only runs when roots are requested or inferable from raw full-path tables;
these filesystem observations are not integrity or run-coverage evidence.

Comparison rejects schema, profiling mode, configuration, histogram contract,
expected/observed roster or verified-completeness mismatches. Different run IDs and
generations are expected for different experiments and do not themselves prevent
comparison. Compatible comparisons show exact operation counter deltas, ratios
(null for zero baseline), errno and percentile evidence for both sides. Direct
report JSON is accepted; passing a generation directory additionally pins it once
and verifies every manifest checksum before comparison.

## Atomic publication

Files and a SHA256 inventory are staged, fsynced and checked. The generation is
renamed into an immutable `generations/` directory before the single
`<prefix>-current` symlink is switched atomically. Compatibility aliases dereference
that pointer. Readers needing multi-file consistency must resolve the pointer once.
No old generation is deleted; failed switching leaves the prior pinned inventory
readable. Unmanaged existing compatibility files are not overwritten. This is not
a full directory-fsync power-loss durability protocol.

## Verification

Strict vertical RED/GREEN evidence is appended to
`/tmp/copper-reporting-test-log.md`. Tests compile a real C++20 writer against
`src/metric/profiler_core.cpp`, exercise heavy-hitter schema3 byte-level bundles,
and consume actual `snapshot_and_reset()` interval plus cumulative output. CLI
subprocesses generate and compare real reports. Regression matrices cover checksum
corruption, malformed histogram bins, uint64/CSV/errno validation, run/generation
ambiguity, epoch overlap/gaps/regression, operation/histogram regression, compatibility
mismatches and failed atomic switching. Run:

```sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

CMake/launcher installation and distributed mounted-runtime validation remain owned
by the parent integration worker; these tests establish local offline integration,
not distributed timing or workload equivalence.
