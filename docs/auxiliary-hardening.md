# Auxiliary safety hardening

## Shutdown: integration contract (deliberately fail closed)

`stop_copper_aurora.sh` and `stop_copper_frontier.sh` now delegate to
`auxiliary_shutdown.py`. Name-based `pkill` is removed. Legacy `-d/-v`
callers resolve the exact scheduler job directory to `daemon-identity.json`;
`-v` must match every manifest record. There is **no fallback process discovery**.

```
bash scripts/stop_copper_aurora.sh --manifest /shared/job/daemon-identity.json --job-id 123.server
bash scripts/stop_copper_frontier.sh --manifest /shared/job/daemon-identity.json --job-id 123
```

Both production launchers now publish this manifest atomically after the
readiness barrier. `cu_fuse_init` captures the **actual post-daemonization daemon**,
not the launcher, shell, srun, or pre-fork PID, after engine initialization.
The launcher creates a fresh caller-owned 0700 host-record directory, exports
`COPPER_JOB_ID`, `COPPER_RUN_ID` and a fresh `COPPER_DAEMON_GENERATION` to every
rank, and collects one private 0600 record per exact scheduler hostname.
The directory path is `COPPER_DAEMON_IDENTITY_DIR`; the job directory is 0700.
A missing, stale-generation, mismatched-job, unsafe, or mountless record fails
launch closed. A new launch invalidates the previous job manifest before launch.
Startup summaries retain the same run and generation used by schema3 profiling.
The complete opaque PBS ID is retained in the manifest even though the historical
log-directory component strips the scheduler suffix. Manifest file
must be regular, caller-owned, not a symlink, and not group/world writable.
Job IDs are exact opaque strings; no scheduler suffix stripping is performed.

```json
{
  "schema_version": 1,
  "job_id": "123.server",
  "expected_hosts": ["host-exactly-as-socket-gethostname"],
  "nodes": {
    "host-exactly-as-socket-gethostname": {
      "pid": 12345,
      "uid": 1000,
      "user": "username",
      "starttime": 987654,
      "boot_id": "contents of /proc/sys/kernel/random/boot_id",
      "exe": "/absolute/resolved/path/cu_fuse",
      "mount": "/tmp/username/copper",
      "mount_identity": {"mount_id": 42, "fstype": "fuse.cu_fuse", "source": "cu_fuse"}
    }
  }
}
```

`starttime` is field 22 of `/proc/PID/stat`, parsed after the last `)`;
`exe` is `/proc/PID/exe`. Capture mount identity using
`auxiliary_shutdown.exact_mount(mount)` in the daemon's mount namespace after
readiness. `mount_identity: null` is only appropriate when no mount was created.
`-vpath` and the exact mount must be adjacent arguments in the daemon cmdline.
Hosts are not shortened or aliased. UID, username, executable, boot, PID,
start time, and mount argument are checked before pidfd acquisition and again
before each signal. `/proc/self/mountinfo` checks exact mountpoint and rejects
stacked mounts; a containing root filesystem is not an active Copper mount.
A currently absent mount is allowed; a different active mount is rejected.

Only the bound pidfd gets TERM then (if needed) KILL. Linux libc pidfd APIs are
required; this Python build lacks `os.pidfd_open` and `signal.pidfd_send_signal`,
so the implementation uses libc, never a racy kill-by-PID fallback. After exit,
only a still-matching FUSE mount gets non-lazy `fusermount3 -u`. Final mount state
is read back. No recursive deletion, mount-directory removal, broad signals,
lazy unmount, or success suppression occurs. Missing/recycled/dead identities
fail conservatively rather than pretending cleanup succeeded.

Distributed mode invokes clush or one-host srun sequentially, with a transport
timeout. It validates one JSON receipt per expected host and returns an aggregate
`nodes` map; any remote/transport/receipt/cleanup failure is nonzero. `--local`
executes on the current host and prints one receipt. `--timeout` defaults to 10
seconds per process wait/unmount, transport budget is `3*timeout+10`. Redirect
stdout to retain the JSON receipts. Validation errors before dispatch go to
stderr. Receipts report `directory_removed: false`; an unreachable host remains
failed, never inferred clean.

**Limits:** manifest authenticity rests on trusted launcher/caller ownership;
this is not protection from malicious processes of the same UID. Mount-table
check and fusermount invocation cannot be atomic against same-UID concurrent
remounts. Run shutdown in the same PID/mount namespace with no concurrent launch
or remount. Shared script/manifest paths must exist on every host. No real Copper
mount, clush allocation, or srun allocation was used in these tests.

## IOCTL commands and timeout wrapper

```
bash scripts/get_copper_stats_ioctl/run_ioctl.sh data-cache-size --device /mount/.spec_ioctl_file
bash scripts/get_copper_stats_ioctl/get_all_metrics.sh --device /mount/.spec_ioctl_file
bash scripts/get_copper_stats_ioctl/reset_fs.sh --device /mount/.spec_ioctl_file --allow-mutation
```

`SPEC_DEV_VIEW` supplies a default device; no env.sh is sourced and no file or
parent directory is implicitly created. Numeric request inputs are rejected.
Read-only-to-cache commands: `cache-tables` (200), `operation-count` (300),
`operation-time` (400), `cache-hits` (500), `cache-misses` (600), `all-metrics`
(2000), `data-cache-size` (2300), `tree-cache-size` (2400),
`metadata-cache-size` (2500). Mutating commands: `clear-cache` (100), `reset-fs`
(1900), both requiring `--allow-mutation`. These codes were traced through
`src/fs/constants.h` and `src/fs/cu_fuse.cpp`. Read commands still cause Copper
to write report files and account operations; “read-only” means no cache/reset
mutation, **not** zero side effects.

CLI opens read commands O_RDONLY and mutations O_RDWR; symlink devices are
rejected. JSON receipts include request, mutating classification, status, and
errno. Open/ioctl/argument failures are nonzero. All shell IOCTL entrypoints
use `timeout_ioctl.py`, which bounds the child open+ioctl using
`COPPER_IOCTL_TIMEOUT` (positive finite seconds, default 10). Timeout exits 124
and says completion is unknown: never automatically retry a reset. Direct
`ioctl.py` calls are not timeout-wrapped. Uninterruptible kernel I/O cannot be
guaranteed killable by a userspace deadline.

`get_all_metrics.sh` no longer copies arbitrary stale `.output` files.
`reset_fs.sh` never removes prior reports. `get_cache_usage_summary.sh` is now
**offline summary only** and propagates errors; collection and analysis are
explicitly separate. Old output-directory positional collection calls must be
updated. Successful ioctl status alone does not prove the daemon durably wrote
a coherent report; report validation is separate.

## Cache summary: immutable generation contract

```
bash scripts/get_copper_stats_ioctl/get_cache_usage_summary.sh /bundle-root \
  --run-id run-123 --expected-host hostA --expected-host hostB --json --csv summary.csv
```

Publisher contract: write immutable per-host snapshot directories, data first,
then atomically publish `cache_snapshot.json` last:

```json
{"schema_version":1,"run_id":"run-123","host":"hostA","epoch":0,"sequence":2,
 "tables":{"data":{"path":"data.output","sha256":"SHA256_HEX"},
           "tree":{"path":"tree.output","sha256":"SHA256_HEX"},
           "metadata":{"path":"metadata.output","sha256":"SHA256_HEX"}}}
```

Data retains production `Total bytes: N` and `total number of files: N` or
`total number of entries: N` lines. Exactly one of each is required. The consumer
requires explicit unique expected hosts and run ID; rejects mixed runs/epochs,
duplicate `(host,epoch,sequence)`, unexpected hosts, invalid integer generation
fields, incomplete table maps, unsafe paths and selected payload hash mismatch.
It selects greatest sequence once per host, never sums periodic plus final
snapshots. Sequence numbers need not match across hosts: this is not a globally
simultaneous measurement. Selected file contents are hashed and parsed from the
same bytes. Unselected payloads are not read/verified. Missing hosts produce
partial counts **and nonzero exit**. JSON and CSV retain verification/coverage.

`Util::log_all_metrics` now writes the traditional reports and publishes an
immutable checksummed three-table generation under `tables/cache-snapshots/`.
The final `Util::log_profiling_metrics` path also publishes cache occupancy, even
when profiling counters are disabled, before cache destruction. It uses real
Data/Tree/Metadata formatters, not an offline exporter or relabeled stale files.
All three payload files are fsynced; `cache_snapshot.json` is written last, the
staging directory is fsynced and renamed atomically, and the parent is fsynced.
Writer/open/flush/fsync failures return a failed `ReportResult`; the all-metrics
IOCTL returns `-EIO`, and individual IOCTL report streams check flush/close.
Final destroy has a void FUSE callback and cannot return an IOCTL status.

The cache-specific capture epoch is 0 (occupancy publication, not profiler reset
intervals), with monotonically increasing process-local sequence. Run ID and
daemon generation must be present. Consumers must be given the run-specific
root: mixed runs and duplicate host sequences across restarts fail closed.
Each table is separately copied under its own cache lock. The manifest records
`capture_semantics: per-table-locked-copy`: **the three tables are not captured
at one globally coherent instant**, and hosts are not globally synchronized.
Existing metadata formatting reports 144 bytes per entry (historical estimate),
not allocator footprint; tree bytes count stored child-name characters, not
container overhead. These are occupancy estimates, not capacity.
This manifest is separate from profiler bundle manifests.

For old tables, use `--legacy --expected-host HOST ...` (no run ID required).
Both `HOST-PID-suffix` and `HOST-suffix` are recognized against explicit expected
hosts. Multiple candidate files for a host/table and mixed PID prefixes across a host's tables are rejected. Legacy results
always say `legacy-unverified`: filename coverage is not proof of same-run,
same-epoch, or coherent snapshot membership. Tables are occupancy, not remaining
capacity or a fixed budget.

## Path observations versus inventories

```
python3 scripts/path_usage_analysis.py /job/logs --memory-bounded
python3 scripts/path_usage_analysis.py /job/logs --inventory saved.jsonl --memory-bounded
python3 scripts/path_usage_analysis.py /job/logs --walk --usage-root /specific/environment \
  --max-entries 100000 --max-depth 32 --max-seconds 30 --save-inventory saved.jsonl --memory-bounded
```

Default behavior reads historical full-path `.output` observations only. It
never walks inferred conda/home roots, stats observed paths, or claims that a
path absent now was absent during the run. Raw path tables remain explicitly
`legacy-unverified`; attempted operations are observations, not proof of success
or future need. Outputs use `observed_*` and `inventory_not_observed` names,
not prune/missing-probe claims. No deletion recommendations are made.

Saved inventory JSONL: first record has `schema_version:1`,
`kind:"path-inventory"`, `captured_at`, absolute `roots`, boolean `complete`,
and optional validated `entry_count`; subsequent records have absolute normalized
`path` and `type` (`file`, `directory`, `symlink`, `other`). Replay uses inventory
records only, not the live filesystem. Capture time is not asserted to equal
profile time. Duplicate included paths and invalid records fail. A partial
inventory remains partial on replay and produces nonzero exit.

Live walks require explicit roots and a saved-inventory destination; `/` is
rejected. Walks have entry/depth/cooperative elapsed-time bounds, do not recurse
through encountered symlinks, and mark truncation/errors as incomplete/nonzero.
Saved inventory is atomically replaced. Traversal is not a filesystem snapshot;
concurrent path replacement can change results, and blocked network filesystem
syscalls cannot be bounded by the cooperative elapsed check. Prefer offline
saved inventories on unstable filesystems.

`--memory-bounded` uses temporary disk-backed SQLite primary-key indexes and
streamed output, a 2 MiB SQLite page cache and disk-backed traversal queue.
Memory scales with individual input records, not the total path set; local disk
space must be sufficient. Default mode uses in-memory SQLite. Legacy line-format
observations cannot unambiguously represent newlines or paths containing ` {`.

## Verification

Strict vertical RED/GREEN executions are recorded in
`/tmp/copper-aux-test-log.md`. Run:

```
python3 -m unittest discover -s tests -p 'test_auxiliary*.py'
```

Tests execute production CLIs and wrappers, reject malformed identity/generation
inputs, use disposable Python processes for pidfd termination (an unrelated
process stays alive), exercise a blocking FIFO timeout, replay saved inventories,
and verify bounded symlink-safe traversal. Real FUSE IOCTL success, active FUSE
unmount, and facility scheduler behavior require allocation-backed integration
testing. No live Copper process was signaled and no commits were made.

Production-integration RED/GREEN evidence is in `/tmp/copper-integration-test-log.md`.
`tests/test_daemon_identity.cpp` forks and proves the record binds the child PID.
`tests/test_runtime_auxiliary_integration.py` compiles the actual Util/cache writer,
feeds its immutable generations to the production summary consumer, and exercises
write-failure propagation through the extracted actual FUSE IOCTL callback. Only
libfuse enum/metric-boundary dependencies are shimmed in this dependency-light
test; Thallium/libfuse startup and kernel IOCTL success are not simulated as live
evidence. A real disposable C++ identity producer is verified and terminated by
the real shutdown CLI while an unrelated process remains alive. Launcher tests
use scheduler transport fixtures and assert readiness, private exact-job manifests,
environment propagation, missing identity failure, and Frontier equivalents.
Frontier now runs daemonizing ranks through a bounded srun step and uses the
same clush readiness barrier; clush must be available there. Full facility-backed
launch, FUSE init/mountinfo timing, namespace equality and shutdown still require
allocation tests. Concurrent launches sharing a job directory are unsupported.
