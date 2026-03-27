# Path Usage Analysis

- Log directory: `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/scale_test/2_nodes/copper-logs-dir/8405873`
- Raw full-path input directory: `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/scale_test/2_nodes/copper-logs-dir/8405873/tables/final`
- Output directory: `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/scale_test/2_nodes/copper-logs-dir/8405873/paths_dir`

## Selected roots

- `/lus/flare/projects/datascience/kaushik/copper-tests/aurora_runs/packages-venv`

## In Simple Terms

- `all_possible_existing_paths.txt`: the full existing path universe under the selected roots for this analysis.
- `used_paths_existing.txt`: paths that `import torch` actually touched and that really existed.
- `same_run_candidate_not_observed_existing_paths.txt`: paths that existed in the same area but did not show up in this run. This is the "maybe safe to prune for the exact same rerun" list.
- `missing_probe_paths.txt`: paths that Python or native loaders checked for, but that were already not there. These are usually probe misses, not deletion candidates.

The best short mental model is:

- `all_possible` = the local universe under the selected roots
- `used` = keep
- `candidate_not_observed` = prune candidates for the same rerun
- `missing_probe` = normal misses that were already absent

## Counts Table

| Meaning | File | Total Paths | Files | Directories | Missing |
|---|---|---:|---:|---:|---:|
| All possible existing paths under the selected roots | `all_possible_existing_paths.txt` | 22383 | 20625 | 1758 | 0 |
| Actually used in this run | `used_paths_existing.txt` | 1729 | 1404 | 325 | 0 |
| Not seen in this run, but exists | `same_run_candidate_not_observed_existing_paths.txt` | 20654 | 19221 | 1433 | 0 |
| Probed but absent | `missing_probe_paths.txt` | 555 | 0 | 0 | 555 |

## Interpretation Notes

- Observed paths under the selected roots in this run: `2284`
- Existing observed paths: `1729`
- Missing probe paths: `555`
- Existing paths not observed in this run: `20654`
- All existing paths under the selected roots: `22383`

## Coverage Estimates

- Observed files: `1404` of `20625` (`6.81%`)
- Observed directories: `325` of `1758` (`18.49%`)

This is not a proof of safe deletion. It is only a same-app, same-node-count, same-config candidate list. A later run can still need a path that did not appear here.
