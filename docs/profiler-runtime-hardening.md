# Profiler runtime schema 3

## Compatibility and integration

The completion API and the first three `ProfilerConfig` aggregate initializer fields
are preserved. A fourth field `bool heavy_hitters=false` selects Space-Saving path
tracking. The runtime constructor also accepts `COPPER_PROFILE_PATH_MODE`:
`heavy-hitter` or `admitted-set` (unset/empty preserves config). Unknown nonempty
values throw `std::invalid_argument`. Heavy-hitter requires positive `top_n` and
`full_paths=false`; invalid combinations fail explicitly. `top_n` is limited to
1,000,000 and reservoir capacity to 65,536. Full-path remains intentionally
unbounded in distinct paths. Parent CLI integration can export the new environment
variable alongside the existing top-N capacity option. Profiling must already be
enabled: environment mode does not silently activate disabled profiling.

`Profiling::mode_string()` uses the actual collector snapshot mode, not a guessed
label derived from constants. No external crypto library or linker flag is needed.
The parent owns CMake benchmark registration and downstream schema-3 support.

## Latency

`LatencySnapshot::histogram` is an exact observation-count histogram with fixed
integer nanosecond bins, independent of the legacy reservoir capacity. There are
65 **inclusive** bins: index 0 `[0,0]`, index 1 `[1,1]`, index b (2 through 63)
`[2^(b-1),2^b-1]`, index 64 `[2^63,UINT64_MAX]`. Sample provenance is
`all_completions`; histogram count equals operation completion count. Zero-duration
observations are retained. Counts, cumulative latency, extrema and errors remain
integers. Overflow of completion cumulative latency/count and report aggregate
sums fails explicitly rather than emitting wrapped totals. Invalid enum values
fail before array indexing; the legacy out-of-range operation no-op is preserved.

`LatencyHistogram::merge` adds compatible bins with checked overflow before
mutation. `quantile_upper_bound(numerator,denominator)` returns the upper bound
of the nearest-rank bucket, using integer arithmetic; empty histogram returns 0.
Within a nonzero bucket the estimate overstates the true quantile by at most that
bucket's width, less than a factor of two. Report both bin bounds if exact latency
is needed. The overflow bucket has an explicit finite uint64 upper bound.

The original operation CSV quantiles remain reservoir estimates and preserve the
`sample_count,sampling_method` columns (`reservoir_algorithm_r`). These **must not
be averaged** across nodes, time intervals, or operations. Merge histogram counts
first and derive global bucket quantiles. Reservoir samples themselves are not
serialized or advertised as mergeable. All-completion histograms do not have
reservoir sampling error; they do have logarithmic discretization error.

## Path modes and accounting

Modes are `aggregate-only`, `bounded-admitted-set`, `bounded-heavy-hitter`, and
`full-path`. The first admitted-set retains the first K `(source,path)` keys,
counts them exactly, and routes nonadmitted events to `other_events`. It is not
a top-K algorithm. Full-path retains all keys exactly. Aggregate-only retains no
path keys; nonempty-path events still count in `paths_seen` and `other_events`.

Heavy-hitter uses deterministic Space-Saving replacement: replace a minimum
estimated-count entry on a miss at capacity. New estimate starts at the victim's
estimate plus the new observation; `error_bound` is the victim's old estimate.
For a retained key with true epoch frequency F:

`estimated_total - error_bound <= F <= estimated_total`.

The standard Space-Saving error is at most floor(N/K), N being all nonempty-path
observations in the epoch; keys with frequency greater than N/K are retained.
Selection is approximate, not guaranteed exact top-K. Victim lookup is O(K),
retained lookup O(log K), memory O(K) keys (path lengths/error-map entries also
consume memory). This simple implementation deliberately does not claim scalable
sharding or constant-time replacement.

`total_events`/`PathSnapshot::total`, outcome, TTL and errno columns count **exact
observations since the key's latest admission**, not estimates of the whole epoch.
Evicted exact observations move once to `other_events` and `paths_dropped`.
Thus `sum(retained.total_events)+other_events == paths_seen` in every mode.
`paths_seen` counts events, NOT distinct paths; `paths_retained` counts keys;
`paths_dropped == other_events` counts unretained exact events, NOT unique keys.
Estimated totals may sum to N, but must never be added to exact retained counters
or used as outcome counts. Results are sorted by estimated total, then source/path.
The legacy exact modes set estimated_total=total_events and error_bound=0.
Do not merge independently derived heavy-hitter estimates as exact path counts.

## Snapshot epochs and identity

`snapshot()` is cumulative **since the most recent reset**, not since process
start. `snapshot_and_reset()` returns a closed interval and resets operations,
reservoirs, histograms, paths, TTL and accounting under the same mutex. It labels
the returned snapshot `semantics="interval"`. `reset_epoch` starts at 0; the
returned interval carries the old epoch and the next cumulative snapshot carries
the incremented epoch. Formatting and filesystem I/O occur outside the collector
lock. Report sequence is process-local and does not prove a restart identity.

`COPPER_RUN_ID` and `COPPER_DAEMON_GENERATION` supply opaque manifest strings.
Unset/empty values serialize as JSON `null`, truthfully unknown, never invented
from host/PID. Set them consistently before daemon launch and do not mutate them
while reporting. Consumers must not sum overlapping cumulative snapshots in one
(run_id,daemon_generation,host,pid,reset_epoch): choose an appropriate latest
snapshot or calculate validated differences. An interval and a cumulative
snapshot of the same epoch overlap and must not both be counted. Null identities
must not be assumed to establish cross-process continuity.

## Exact bundle schema

Manifest `schema_version` is **3**. Existing `sequence`, `kind`,
`timestamp_unix_ms`, `host`, `pid`, `config`, and path-accounting fields remain.
Added fields:

- `run_id`, `daemon_generation`: string or null.
- `semantics`: `cumulative` or `interval`; `reset_epoch`: uint64 integer.
- `mode`: one of the four actual collector modes above.
- `histogram`: `{ "method":"fixed_log2_integer", "sample_provenance":"all_completions", "bucket_count":65, "bounds":"inclusive", "unit":"ns" }`.
- `file_integrity`: object mapping every exact data basename to
  `{ "sha256":"<64 lowercase hex characters>", "size_bytes":<integer> }`.

`files` remains a string list, now exactly these four data basenames (same stem
`<sanitized-host>-<pid>-<sequence>-<sanitized-kind>`):

1. `<stem>-profiling_operations.csv`: original header unchanged.
2. `<stem>-profiling_top_paths.csv`: original header plus `estimated_total,error_bound`.
3. `<stem>-profiling_aggregate.csv`: original `metric,value` header/metrics unchanged.
4. `<stem>-profiling_histogram.csv`: exact header
   `operation,bucket_index,lower_bound_ns,upper_bound_ns,count`; all 65 bins,
   including zero-count bins, for each operation with completions.

Validate the schema, file identity/membership, byte size and SHA256 before parsing
CSV. Hashes cover actual binary output bytes including final newline and escaped
CSV paths. SHA256 is dependency-free FIPS 180-4, tested against empty, abc and
million-a known vectors. It is integrity detection, not authentication.

Data files are atomically renamed first, manifest last (commit marker). Never
consume orphan CSV files. Duplicate published bundle identities are rejected.
A per-identity `.publishing` claim directory excludes concurrent writers of the
same identity; a crash may leave a claim requiring operator investigation/removal.
Failed writes return `ReportResult{success=false,error=...}` and no successful
manifest claim. This is atomic publication, not a power-loss durability guarantee:
there is no fsync protocol. Readers still verify checksums to detect corruption.

## Measured overhead

Build and test without CMake (CMake integration is parent-owned):

```sh
mkdir -p /tmp/copper-profiler-work
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread -Isrc tests/benchmark_profiler.cpp src/metric/profiler_core.cpp -o /tmp/copper-profiler-work/benchmark-profiler
python3 tests/test_profiler_benchmark.py -v
python3 tests/test_profiler_benchmark.py --run-matrix --binary /tmp/copper-profiler-work/benchmark-profiler --output /tmp/copper-profiler-work/benchmark.csv --iterations 100000 --snapshots 100 --timeout 120
```

The Python test uses `COPPER_PROFILER_BENCHMARK` if provided (for a parent-built
binary), otherwise compiles the actual C++ benchmark/core into a temporary
directory. It uses only the Python standard library. Six acceptance tests passed;
RED was observed before each implementation slice: missing CSV columns,
ignored case options, zero snapshot count, missing runner, nonfinite runner
timeout, and inherited parent RSS. The tests exercise all 20 mode/thread cases,
completion and retained-path counts, finite positive measurements, strict option
validation, matrix persistence and process memory isolation.

### Measurement contract

One binary invocation runs **one case**, not an in-process matrix. For example:

```sh
/tmp/copper-profiler-work/benchmark-profiler --mode bounded --threads 8 --iterations 100000 --snapshots 100
```

Modes are `disabled`, `aggregate`, `admitted` (bounded admitted-set), `bounded`
(bounded heavy-hitter), and `full`. Threads are restricted to 1, 2, 4 or 8.
Default case is aggregate/1 thread/100000 iterations/10 snapshots. Numeric CLI
values are positive decimal integers, iterations at most 1000000000, snapshots
at most 1000000; signs, fractions, trailing junk, nonfinite values, overflow,
unknown/missing/duplicate options and unsupported threads fail with status 2 and
no CSV. These bounds also keep workload/checksum arithmetic within uint64.
Runner timeout must be finite and positive and its counts are range checked
before opening the output file.

The CSV contract is:

```text
mode,threads,iterations_per_thread,total_calls,elapsed_ns,ns_per_call,paths_retained,snapshot_count,snapshot_elapsed_ns,ns_per_snapshot,process_peak_rss_kib
```

- `total_calls` is verified recorded completions: zero when disabled, otherwise
  threads times iterations. `ns_per_call` divides completion-loop wall time by
  **attempted** calls, including for disabled mode. It is throughput-amortized
  cost, **not request latency**. The timed region includes gate release and join,
  not thread creation or path construction.
- `snapshot_elapsed_ns` sums elapsed time immediately around `core.snapshot()`
  after workers join; `ns_per_snapshot` divides by `snapshot_count`. This covers
  copying and finalization/sorting, not CSV/report serialization, hashing or I/O.
  Snapshots are cumulative, repeated on the same quiescent epoch, and do not
  reset the collector. Previous snapshot destruction/move assignment is outside
  timing; peak memory can include previous and current snapshot allocations.
- `paths_retained` is the last snapshot's actual retained key count.
- `process_peak_rss_kib` is `getrusage(RUSAGE_SELF).ru_maxrss` after snapshots
  (Linux KiB; macOS bytes converted to KiB), **whole-process lifetime peak resident
  memory**, not profiler-only bytes, not a current-RSS delta and not virtual
  memory. It includes runtime, path fixtures, thread resources, collector and
  snapshots, and excludes subsequent output formatting. macOS conversion is
  implemented but this acceptance run was on Linux.

Each matrix case starts a fresh process, sequentially. A direct Python fork/exec
was experimentally found to carry the Python process's RSS peak into the case:
all first-run measurements read 21536 KiB; an 80 MB parent allocation raised a
probe to 93432 KiB. The runner therefore launches a small `/bin/sh` which forks
rather than exec-replaces itself with the case. Its trailing status command
preserves exit status and prevents shell exec optimization. The benchmark then
starts from the small shell's current footprint rather than the Python parent's
high-water mark. A 64 MiB parent-allocation regression fails without this launch
isolation and passes with it. The raw CSV below was regenerated after that fix;
the polluted first-run numbers are not used as profiler memory evidence.

The runner verifies schema, exactly one row per invocation, case/count identity,
finite positive costs and all 20 distinct cases; it flushes each verified raw row
to the CSV. A failed/interrupted run can leave a partial CSV, so require the
successful `Verified 20 independent cases` message and validate row membership
before treating an output file as a complete matrix.

### Actual execution

Measured on 2026-11-01, Linux x86_64, Intel Core i7-14700, GCC 13.3.0, `-O2`,
100000 attempted completions per thread and 100 snapshots per case. Raw data:
`/tmp/copper-profiler-work/benchmark.csv`. The table is rounded from those actual
20 rows; raw CSV retains elapsed integer nanoseconds and six decimal places for
amortized costs.

| mode | threads | ns/call | snapshot ns/each | process peak RSS KiB | retained paths |
|---|---:|---:|---:|---:|---:|
| disabled | 1 | 1.92 | 320.05 | 3644 | 0 |
| disabled | 2 | 0.98 | 318.53 | 3644 | 0 |
| disabled | 4 | 0.60 | 326.80 | 3588 | 0 |
| disabled | 8 | 0.36 | 312.95 | 3624 | 0 |
| aggregate | 1 | 80.27 | 12616.14 | 3624 | 0 |
| aggregate | 2 | 171.93 | 44918.33 | 3588 | 0 |
| aggregate | 4 | 180.13 | 75085.84 | 3180 | 0 |
| aggregate | 8 | 229.11 | 82289.86 | 3648 | 0 |
| admitted | 1 | 81.02 | 20701.73 | 3388 | 64 |
| admitted | 2 | 170.70 | 39514.91 | 3344 | 64 |
| admitted | 4 | 231.17 | 100345.63 | 3600 | 64 |
| admitted | 8 | 334.62 | 103000.67 | 3596 | 64 |
| bounded | 1 | 171.35 | 18734.82 | 3392 | 64 |
| bounded | 2 | 351.61 | 41960.90 | 3344 | 64 |
| bounded | 4 | 400.23 | 95680.25 | 3608 | 64 |
| bounded | 8 | 465.46 | 92312.84 | 3648 | 64 |
| full | 1 | 98.46 | 189376.27 | 4240 | 1024 |
| full | 2 | 219.97 | 164908.92 | 4288 | 1024 |
| full | 4 | 271.23 | 194185.32 | 4544 | 1024 |
| full | 8 | 349.65 | 183923.15 | 4544 | 1024 |

### Evidence and limits

The same fixed workload is used in every case: 1024 prebuilt paths, 80% accesses
to 16 hot keys and 20% rotating cold keys, bounded capacity 64, reservoir capacity
1024 per operation. Threads use four operation IDs (`thread % 4`); thus increasing
from one to four threads also fills more operation reservoirs. The disabled
case still constructs a collector/fixtures and executes a runtime branch plus
non-elidable local work; it is a harness baseline, not production disabled RSS.
No filesystem, callback clock capture, RPC, daemon integration or report output
is measured. CPU affinity, frequency and machine load were not controlled.

Aggregate and bounded modes show higher amortized cost under contention than
at one thread. Full-path retains 1024 keys instead of 64 and has a larger measured
process RSS and snapshot cost here. At one thread, full snapshot cost was about
10.1 times the heavy-hitter snapshot cost. Full-path completion can nevertheless
be faster than heavy-hitter on this finite workload: the latter scans victims
at capacity. This is evidence of tradeoffs, not proof that one algorithm wins
for all workloads, or a precise attribution of time to lock versus victim scan.

This is **one run per case**, not a statistical bound or a production capacity
claim. RSS page granularity, allocator reuse, shared libraries, thread stacks
and scheduling can obscure small differences; do not subtract the disabled
measurement and label the difference collector allocation. Full-path memory is
unbounded in distinct keys, but this experiment only exercises 1024 short
keys. Repeated quiescent snapshots benefit from warm caches and do not measure
snapshot interference with concurrent writers or reset overhead. No latency
percentiles of snapshot durations or contention were measured.

No speculative sharding was implemented or claimed. These measurements supply
throughput, memory and snapshot baselines before considering sharding; any
alternative still needs repeated, cardinality-scaled and concurrent-snapshot
experiments plus exact coherent-epoch correctness validation.
