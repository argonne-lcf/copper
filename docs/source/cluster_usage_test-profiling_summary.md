# Cluster Profiling Summary

- Input directory: `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/scale_test/2_nodes/copper-logs-dir/8405873/profiling/final`
- Rank summaries aggregated: `4`

## Aggregate Metrics

| Metric | Value |
|---|---:|
| Ranks with aggregate files | 4 |
| `estimated_cache_served_operations_saved_by_copper` | 5984151 |
| `estimated_repeated_metadata_lookups_avoided_by_ttl` | 45424 |
| `metadata_enoent_ttl_clears_on_success_or_non_enoent` | 0 |
| `metadata_enoent_ttl_expires` | 0 |
| `metadata_enoent_ttl_serves` | 45424 |
| `metadata_enoent_ttl_stores` | 2372 |
| `total_cache_hits` | 5984151 |
| `total_cache_misses` | 3010 |
| `total_counted_fuse_operations` | 6377844 |
| `total_measured_latency_seconds` | 39.513989 |
| `total_negative_results` | 168147 |

## Operation Totals

| Operation | Total Calls | Cache Hits | Cache Misses | Negative Results | Avg Latency (us) | Total Latency (s) |
|---|---:|---:|---:|---:|---:|---:|
| `destroy` | 4 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `ext_getattr` | 34032 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `flush` | 35376 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `getattr` | 5952916 | 5906788 | 0 | 46128 | 5.27 | 31.390461 |
| `init` | 4 | 0 | 0 | 0 | 88504.75 | 0.354019 |
| `ioctl` | 34032 | 0 | 0 | 0 | 6.63 | 0.225617 |
| `lock` | 35376 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `open` | 35376 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `opendir` | 6480 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `read` | 195912 | 71307 | 2586 | 122019 | 35.77 | 7.008104 |
| `readdir` | 6480 | 6056 | 424 | 0 | 82.68 | 0.535788 |
| `release` | 35376 | 0 | 0 | 0 | 0.00 | 0.000000 |
| `releasedir` | 6480 | 0 | 0 | 0 | 0.00 | 0.000000 |

## Path Class Heuristics

This section groups the hottest observed paths into coarse classes so users can reason about what to prune and what to keep.

| Path Class | Entries | Total Events | Example |
|---|---:|---:|---|
| `environment_prefix` | 160 | 5075764 | `/lus` |
| `shared_library` | 80 | 126484 | `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/packages-venv/nvidia/cu13/lib/libcublasLt.so.13` |
| `negative_probe_path` | 80 | 19452 | `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/nvidia` |

## Pruning Guidance

These are heuristic suggestions derived from the observed top paths and TTL paths. They are intended to guide a staged cleanup, not to justify deleting unseen files from the original environment.

- Keep the active environment core paths intact first: environment root, `bin`, `lib`, `lib/python*`, and `site-packages`.
- Repeated ENOENT probe paths are present. High TTL serves suggest it is worth auditing `LD_LIBRARY_PATH`, `PATH`, and `PYTHONPATH` for duplicate or stale entries, while keeping the ENOENT TTL enabled.
- Recommended pruning order: remove duplicate entries first, then nonexistent entries, then stale environment/toolchain paths, and only then experiment with a reduced copy or allowlist-based tree.
- If running a second minimization experiment, use the observed paths as an initial allowlist and test in a cloned or filtered environment instead of pruning the original tree in place.

## Path Coverage Analysis

This section estimates how much of selected filesystem roots appeared in the raw final full-path table outputs. It is most useful when `-profile_paths_full` was enabled.

| Root | Exists | Observed Paths | Observed Files | Total Files | File Coverage | Observed Dirs | Total Dirs | Dir Coverage | Missing Probe Paths | Example |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/packages-venv` | yes | 2284 | 1404 | 20625 | 6.81% | 325 | 1757 | 18.50% | 555 | `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/packages-venv` |
| `/opt` | yes | 0 | 0 | 1536555 | 0.00% | 0 | 171552 | 0.00% | 0 | `` |
| `/home/kaushikvelusamy/.local` | yes | 0 | 0 | 79 | 0.00% | 0 | 1 | 0.00% | 0 | `` |
| `/home/kaushikvelusamy/.condarc` | yes | 0 | 0 | 0 | n/a | 0 | 0 | n/a | 0 | `` |
| `/home/kaushikvelusamy/.conda` | yes | 0 | 0 | 5 | 0.00% | 0 | 3 | 0.00% | 0 | `` |
| `/home/kaushikvelusamy/.cache` | yes | 0 | 0 | 179 | 0.00% | 0 | 363 | 0.00% | 0 | `` |

This coverage is observational, not a proof of safe deletion. Unobserved files may still be needed on a later run if the code path, timing, or environment changes.
