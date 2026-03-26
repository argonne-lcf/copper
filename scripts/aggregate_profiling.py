#!/usr/bin/env python3

import argparse
import csv
import glob
import os
import re
import sys
from collections import defaultdict


def usage():
    print("usage: aggregate_profiling.py <log_dir> [output_prefix]")


def read_aggregate_csv(path):
    data = {}
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = row["metric"]
            value = row["value"]
            try:
                if "." in value:
                    data[key] = float(value)
                else:
                    data[key] = int(value)
            except ValueError:
                data[key] = value
    return data


def read_operations_csv(path):
    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def total_events(row):
    return int(row.get("total_events", 0))


def classify_path(category, path):
    if category == "metadata_ttl":
        if "python" in path and path.endswith(".zip"):
            return "python_zip_probe"
        if path.endswith("/pyvenv.cfg"):
            return "virtual_env_probe"
        if "glibc-hwcaps" in path:
            return "glibc_hwcaps_probe"
        if ".so" in path:
            return "missing_shared_library_probe"
        return "negative_probe_path"

    if "/site-packages/torch/lib/" in path:
        return "torch_native_library"
    if "/site-packages/torch" in path:
        return "torch_python_package"
    if "/site-packages" in path:
        return "python_site_packages"
    if "/lib/python" in path:
        return "python_stdlib"
    if "/bin/" in path:
        return "environment_bin"
    if ".so" in path:
        return "shared_library"
    return "environment_prefix"


def read_top_paths_csv(path):
    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def is_final_rank_file(path):
    name = os.path.basename(path)
    return re.match(r"^[^-]+-\d+-profiling_(aggregate|operations|top_paths)\.csv$", name) is not None


def read_observed_paths(table_dir):
    observed_paths = set()
    if not os.path.isdir(table_dir):
        return observed_paths

    for path in glob.glob(os.path.join(table_dir, "*.output")):
        with open(path, "r", errors="ignore") as f:
            for line in f:
                line = line.rstrip("\n")
                if line.startswith("/"):
                    observed_paths.add(line.split(" {", 1)[0])
    return observed_paths


def infer_conda_roots(observed_paths):
    roots = set()
    for path in observed_paths:
        marker = "/conda_env"
        idx = path.find(marker)
        if idx != -1:
            roots.add(path[: idx + len(marker)])
    return sorted(roots)


def analyze_usage_root(root, observed_paths):
    if not os.path.exists(root):
        return {
            "root": root,
            "exists": False,
            "total_files": 0,
            "total_dirs": 0,
            "observed_files": 0,
            "observed_dirs": 0,
            "observed_missing": 0,
            "observed_total": 0,
            "example": "",
        }

    observed_under_root = {p for p in observed_paths if p == root or p.startswith(root + "/")}
    observed_files = sum(1 for p in observed_under_root if os.path.isfile(p))
    observed_dirs = sum(1 for p in observed_under_root if os.path.isdir(p))
    observed_missing = sum(1 for p in observed_under_root if not os.path.exists(p))

    total_files = 0
    total_dirs = 0
    for _, dirnames, filenames in os.walk(root):
        total_dirs += len(dirnames)
        total_files += len(filenames)

    example = next(iter(sorted(observed_under_root)), "")
    return {
        "root": root,
        "exists": True,
        "total_files": total_files,
        "total_dirs": total_dirs,
        "observed_files": observed_files,
        "observed_dirs": observed_dirs,
        "observed_missing": observed_missing,
        "observed_total": len(observed_under_root),
        "example": example,
    }


def infer_home_roots(observed_paths, home):
    roots = []
    if not home:
        return roots

    normalized_home = os.path.normpath(home)
    seen = set()

    if any(p == normalized_home or p.startswith(normalized_home + "/") for p in observed_paths):
        roots.append(normalized_home)
        seen.add(normalized_home)

    for path in sorted(observed_paths):
        if not (path == normalized_home or path.startswith(normalized_home + "/")):
            continue
        rel = os.path.relpath(path, normalized_home)
        if rel == ".":
            continue
        first = rel.split(os.sep, 1)[0]
        child_root = os.path.join(normalized_home, first)
        if child_root not in seen:
            seen.add(child_root)
            roots.append(child_root)

    return roots


def main():
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("log_dir")
    parser.add_argument("output_prefix", nargs="?", default="cluster")
    parser.add_argument("--usage-root", action="append", default=[],
                        help="additional filesystem root to analyze for observed-path coverage; can be passed multiple times")
    args = parser.parse_args()

    log_dir = os.path.abspath(args.log_dir)
    output_prefix = args.output_prefix
    profiling_final_dir = os.path.join(log_dir, "profiling", "final")
    profiling_cluster_dir = os.path.join(log_dir, "profiling", "cluster")
    tables_final_dir = os.path.join(log_dir, "tables", "final")

    search_dir = profiling_final_dir if os.path.isdir(profiling_final_dir) else log_dir
    aggregate_files = sorted(path for path in glob.glob(os.path.join(search_dir, "*-profiling_aggregate.csv")) if is_final_rank_file(path))
    operations_files = sorted(path for path in glob.glob(os.path.join(search_dir, "*-profiling_operations.csv")) if is_final_rank_file(path))
    top_paths_files = sorted(path for path in glob.glob(os.path.join(search_dir, "*-profiling_top_paths.csv")) if is_final_rank_file(path))

    if not aggregate_files or not operations_files:
        print(f"failed to find profiling csv files under: {search_dir}")
        return 1

    os.makedirs(profiling_cluster_dir, exist_ok=True)

    aggregate_totals = defaultdict(float)
    operation_totals = defaultdict(lambda: defaultdict(float))
    path_class_totals = defaultdict(lambda: {"entries": 0, "total_events": 0, "example": ""})
    metadata_ttl_serves = 0
    missing_shared_lib_probe_events = 0
    python_probe_events = 0
    observed_paths = read_observed_paths(tables_final_dir)

    for path in aggregate_files:
        for key, value in read_aggregate_csv(path).items():
            if isinstance(value, (int, float)):
                aggregate_totals[key] += value

    for path in operations_files:
        for row in read_operations_csv(path):
            op = row["operation"]
            for key in ["total_calls", "cache_hits", "cache_misses", "negative_results", "total_latency_s"]:
                operation_totals[op][key] += float(row[key])

    for path in top_paths_files:
        for row in read_top_paths_csv(path):
            path_class = classify_path(row["category"], row["path"])
            stats = path_class_totals[path_class]
            stats["entries"] += 1
            stats["total_events"] += total_events(row)
            if not stats["example"]:
                stats["example"] = row["path"]

            if row["category"] == "metadata_ttl":
                metadata_ttl_serves += int(row.get("ttl_serves", 0))
                if path_class == "missing_shared_library_probe":
                    missing_shared_lib_probe_events += total_events(row)
                if path_class in {"python_zip_probe", "virtual_env_probe", "glibc_hwcaps_probe"}:
                    python_probe_events += total_events(row)

    coverage_roots = []
    seen_roots = set()
    for root in args.usage_root + infer_conda_roots(observed_paths):
        if root not in seen_roots:
            seen_roots.add(root)
            coverage_roots.append(root)

    home = os.path.expanduser("~")
    for root in infer_home_roots(observed_paths, home):
        if root not in seen_roots:
            seen_roots.add(root)
            coverage_roots.append(root)

    coverage_stats = [analyze_usage_root(root, observed_paths) for root in coverage_roots]

    aggregate_csv_path = os.path.join(profiling_cluster_dir, f"{output_prefix}-profiling_aggregate.csv")
    with open(aggregate_csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["metric", "value"])
        writer.writerow(["ranks_with_aggregate_files", len(aggregate_files)])
        for key in sorted(aggregate_totals.keys()):
            writer.writerow([key, aggregate_totals[key]])

    operations_csv_path = os.path.join(profiling_cluster_dir, f"{output_prefix}-profiling_operations.csv")
    with open(operations_csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["operation", "total_calls", "cache_hits", "cache_misses", "negative_results", "avg_latency_us", "total_latency_s"])
        for op in sorted(operation_totals.keys()):
            total_calls = operation_totals[op]["total_calls"]
            total_latency_s = operation_totals[op]["total_latency_s"]
            avg_latency_us = (total_latency_s * 1000000.0 / total_calls) if total_calls else 0.0
            writer.writerow([
                op,
                int(operation_totals[op]["total_calls"]),
                int(operation_totals[op]["cache_hits"]),
                int(operation_totals[op]["cache_misses"]),
                int(operation_totals[op]["negative_results"]),
                f"{avg_latency_us:.2f}",
                f"{total_latency_s:.6f}",
            ])

    summary_md_path = os.path.join(profiling_cluster_dir, f"{output_prefix}-profiling_summary.md")
    with open(summary_md_path, "w") as f:
        f.write("# Cluster Profiling Summary\n\n")
        f.write(f"- Input directory: `{search_dir}`\n")
        f.write(f"- Rank summaries aggregated: `{len(aggregate_files)}`\n\n")
        f.write("## Aggregate Metrics\n\n")
        f.write("| Metric | Value |\n")
        f.write("|---|---:|\n")
        f.write(f"| Ranks with aggregate files | {len(aggregate_files)} |\n")
        for key in sorted(aggregate_totals.keys()):
            value = aggregate_totals[key]
            if isinstance(value, float) and not value.is_integer():
                f.write(f"| `{key}` | {value:.6f} |\n")
            else:
                f.write(f"| `{key}` | {int(value)} |\n")

        f.write("\n## Operation Totals\n\n")
        f.write("| Operation | Total Calls | Cache Hits | Cache Misses | Negative Results | Avg Latency (us) | Total Latency (s) |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|\n")
        for op in sorted(operation_totals.keys()):
            total_calls = operation_totals[op]["total_calls"]
            total_latency_s = operation_totals[op]["total_latency_s"]
            avg_latency_us = (total_latency_s * 1000000.0 / total_calls) if total_calls else 0.0
            f.write(
                f"| `{op}` | {int(total_calls)} | {int(operation_totals[op]['cache_hits'])} | "
                f"{int(operation_totals[op]['cache_misses'])} | {int(operation_totals[op]['negative_results'])} | "
                f"{avg_latency_us:.2f} | {total_latency_s:.6f} |\n"
            )

        if path_class_totals:
            sorted_classes = sorted(path_class_totals.items(), key=lambda item: item[1]["total_events"], reverse=True)
            f.write("\n## Path Class Heuristics\n\n")
            f.write("This section groups the hottest observed paths into coarse classes so users can reason about what to prune and what to keep.\n\n")
            f.write("| Path Class | Entries | Total Events | Example |\n")
            f.write("|---|---:|---:|---|\n")
            for path_class, stats in sorted_classes:
                f.write(f"| `{path_class}` | {stats['entries']} | {stats['total_events']} | `{stats['example']}` |\n")

            f.write("\n## Pruning Guidance\n\n")
            f.write("These are heuristic suggestions derived from the observed top paths and TTL paths. They are intended to guide a staged cleanup, not to justify deleting unseen files from the original environment.\n\n")
            f.write("- Keep the active environment core paths intact first: environment root, `bin`, `lib`, `lib/python*`, and `site-packages`.\n")
            if "torch_native_library" in path_class_totals:
                f.write("- `torch_native_library` paths are hot. This workload is relying on native Torch shared libraries, so pruning should focus around the environment rather than removing Torch library directories.\n")
            if "python_site_packages" in path_class_totals or "python_stdlib" in path_class_totals:
                f.write("- Python package and stdlib paths are active. If pruning is desired, prefer removing duplicate `PYTHONPATH` entries and unrelated packages before touching the core interpreter tree.\n")
            if metadata_ttl_serves > 0:
                f.write("- Repeated ENOENT probe paths are present. High TTL serves suggest it is worth auditing `LD_LIBRARY_PATH`, `PATH`, and `PYTHONPATH` for duplicate or stale entries, while keeping the ENOENT TTL enabled.\n")
            if missing_shared_lib_probe_events > 0:
                f.write("- Missing shared-library probes were observed. The lowest-risk cleanup is usually to shorten `LD_LIBRARY_PATH`, remove stale runtime/toolchain directories, and avoid duplicate library search roots.\n")
            if python_probe_events > 0:
                f.write("- Python startup probe paths such as `python*.zip`, `pyvenv.cfg`, or `glibc-hwcaps` were observed. These are usually normal probes, so they are better treated as optimization hints than as application bugs.\n")
            f.write("- Recommended pruning order: remove duplicate entries first, then nonexistent entries, then stale environment/toolchain paths, and only then experiment with a reduced copy or allowlist-based tree.\n")
            f.write("- If running a second minimization experiment, use the observed paths as an initial allowlist and test in a cloned or filtered environment instead of pruning the original tree in place.\n")

        if coverage_stats:
            f.write("\n## Path Coverage Analysis\n\n")
            if observed_paths:
                f.write("This section estimates how much of selected filesystem roots appeared in the raw final full-path table outputs. It is most useful when `-profile_paths_full` was enabled.\n\n")
                f.write("| Root | Exists | Observed Paths | Observed Files | Total Files | File Coverage | Observed Dirs | Total Dirs | Dir Coverage | Missing Probe Paths | Example |\n")
                f.write("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n")
                for stats in coverage_stats:
                    if stats["exists"] and stats["total_files"] > 0:
                        file_cov = f"{(100.0 * stats['observed_files'] / stats['total_files']):.2f}%"
                    else:
                        file_cov = "n/a"
                    if stats["exists"] and stats["total_dirs"] > 0:
                        dir_cov = f"{(100.0 * stats['observed_dirs'] / stats['total_dirs']):.2f}%"
                    else:
                        dir_cov = "n/a"
                    f.write(
                        f"| `{stats['root']}` | "
                        f"{'yes' if stats['exists'] else 'no'} | "
                        f"{stats['observed_total']} | "
                        f"{stats['observed_files']} | "
                        f"{stats['total_files']} | "
                        f"{file_cov} | "
                        f"{stats['observed_dirs']} | "
                        f"{stats['total_dirs']} | "
                        f"{dir_cov} | "
                        f"{stats['observed_missing']} | "
                        f"`{stats['example']}` |\n"
                    )
                f.write("\nThis coverage is observational, not a proof of safe deletion. Unobserved files may still be needed on a later run if the code path, timing, or environment changes.\n")
            else:
                f.write("No raw final full-path table outputs were found under `tables/final/`, so filesystem-root coverage could not be estimated.\n")

    print(f"wrote {aggregate_csv_path}")
    print(f"wrote {operations_csv_path}")
    print(f"wrote {summary_md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
