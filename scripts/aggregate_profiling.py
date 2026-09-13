#!/usr/bin/env python3

import argparse
import csv
import glob
import json
import os
import pathlib
import re
import sys
from collections import defaultdict
from dataclasses import dataclass

SUPPORTED_MANIFEST_SCHEMAS = {2}
SUPPORTED_PROFILING_MODES = {"aggregate-only", "bounded-admitted-set", "full-path"}
SUPPORTED_SAMPLING_METHODS = {"reservoir_algorithm_r"}
AGGREGATE_HEADER = ["metric", "value"]
OPERATIONS_HEADER = ["operation", "total_calls", "cumulative_latency_ns", "min_ns", "p50_ns", "p90_ns", "p95_ns", "p99_ns", "p999_ns", "max_ns", "sample_count", "sampling_method", "cache_hits", "cache_misses", "negative_results", "source_none", "source_metadata", "source_data", "source_tree", "source_bypass", "errors"]
OPERATIONS_NUMERIC_FIELDS = [field for field in OPERATIONS_HEADER if field not in {"operation", "sampling_method", "errors"}]
TOP_PATHS_HEADER = ["source", "path", "total_events", "success", "cache_hits", "cache_misses", "negative_results", "ttl_stores", "ttl_serves", "ttl_expires", "ttl_clears", "errors"]
TOP_PATHS_NUMERIC_FIELDS = [field for field in TOP_PATHS_HEADER if field not in {"source", "path", "errors"}]
AGGREGATE_METRICS = {"total_counted_fuse_operations", "total_cache_hits", "total_cache_misses", "total_negative_results", "cumulative_measured_latency_ns", "paths_seen", "paths_retained", "paths_dropped", "other_events", "md_enoent_ttl_stores", "md_enoent_ttl_serves", "md_enoent_ttl_expires", "md_enoent_ttl_clears"}

def filename_component(value):
    """Match ProfilerCore::filename_component for manifest identity checks."""
    sanitized = "".join(c if c.isascii() and (c.isalnum() or c in "-_.") else "_"
                        for c in value)
    while ".." in sanitized:
        sanitized = sanitized.replace("..", "__")
    return sanitized or "unknown"

def valid_v2_manifest(data):
    required = {"schema_version", "sequence", "kind", "timestamp_unix_ms", "host", "pid", "mode", "config", "paths_seen", "paths_retained", "paths_dropped", "other_events", "files"}
    if not isinstance(data, dict) or set(data) != required:
        return False
    integer_fields = ("schema_version", "sequence", "timestamp_unix_ms", "pid",
                      "paths_seen", "paths_retained", "paths_dropped", "other_events")
    if any(type(data[key]) is not int or data[key] < 0 for key in integer_fields):
        return False
    if any(not isinstance(data[key], str) for key in ("kind", "host", "mode")):
        return False
    if data["mode"] not in SUPPORTED_PROFILING_MODES:
        return False
    config = data["config"]
    return (isinstance(config, dict) and set(config) == {"top_n", "latency_capacity", "sampling_method"} and
            type(config.get("top_n")) is int and config["top_n"] >= 0 and
            type(config.get("latency_capacity")) is int and config["latency_capacity"] >= 0 and
            config.get("sampling_method") in SUPPORTED_SAMPLING_METHODS)

@dataclass
class Bundle:
    schema_version: int
    manifest: pathlib.Path
    files: list
    aggregate: pathlib.Path
    operations: pathlib.Path
    top_paths: pathlib.Path

def discover_committed_bundles(directory):
    root = pathlib.Path(directory)
    candidates = []
    candidate_identities = {}
    logical_owners = defaultdict(set)
    schemas = set()
    for manifest in sorted(root.glob("*-manifest.json")):
        try:
            data = json.loads(manifest.read_text())
            schema = data["schema_version"]
            if type(schema) is not int or schema not in SUPPORTED_MANIFEST_SCHEMAS or not valid_v2_manifest(data):
                continue
            names = data["files"]
            stem_suffix = "-manifest.json"
            stem = manifest.name[:-len(stem_suffix)]
            expected = [f"{stem}-profiling_{kind}.csv" for kind in ("aggregate", "operations", "top_paths")]
            if (not isinstance(names, list) or len(names) != len(expected) or
                    len(set(names)) != len(names) or set(names) != set(expected) or
                    any(not isinstance(n, str) or pathlib.Path(n).name != n for n in names)):
                continue
            files = [root / n for n in expected]
            aggregate, operations, top_paths = files
            if not all(p.is_file() for p in files):
                continue
            logical_identity = (data["host"], data["pid"], data["sequence"], data["kind"])
            logical_owners[logical_identity].add(manifest)
            expected_stem = (f"{filename_component(data['host'])}-{data['pid']}-"
                             f"{data['sequence']}-{filename_component(data['kind'])}")
            if stem != expected_stem:
                continue
        except (OSError, ValueError, TypeError, KeyError, StopIteration, json.JSONDecodeError):
            continue
        bundle = Bundle(schema, manifest, files, aggregate, operations, top_paths)
        candidates.append(bundle)
        candidate_identities[manifest] = logical_identity

    file_owners = defaultdict(set)
    bundle_owners = defaultdict(set)
    for bundle in candidates:
        identity = tuple(p.name for p in bundle.files)
        bundle_owners[identity].add(bundle.manifest)
        for path in bundle.files:
            stat = path.stat()
            file_owners[(stat.st_dev, stat.st_ino)].add(bundle.manifest)
    bundles = [bundle for bundle in candidates
               if len(bundle_owners[tuple(p.name for p in bundle.files)]) == 1 and
               len(logical_owners[candidate_identities[bundle.manifest]]) == 1 and
               all(len(file_owners[(p.stat().st_dev, p.stat().st_ino)]) == 1 for p in bundle.files)]
    schemas.update(bundle.schema_version for bundle in bundles)
    if len(schemas) > 1:
        raise ValueError("mixed schema versions in committed profiling bundles")
    return bundles


def usage():
    print("usage: aggregate_profiling.py <log_dir> [output_prefix]")


def read_aggregate_csv(path):
    data = {}
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != AGGREGATE_HEADER:
            raise ValueError(f"unexpected aggregate header in {path}: {reader.fieldnames!r}")
        for row in reader:
            key = row["metric"]
            value = row["value"]
            if key in data:
                raise ValueError(f"duplicate aggregate metric in {path}: {key!r}")
            try:
                data[key] = int(value)
            except (TypeError, ValueError) as exc:
                raise ValueError(f"malformed numeric value in {path}: {value!r}") from exc
    if set(data) != AGGREGATE_METRICS:
        raise ValueError(f"unexpected aggregate fields in {path}: {sorted(data)!r}")
    return data


def validate_manifest_path_counts(bundle, aggregate):
    manifest = json.loads(bundle.manifest.read_text())
    for key in ("paths_seen", "paths_retained", "paths_dropped", "other_events"):
        if manifest[key] != aggregate[key]:
            raise ValueError(
                f"manifest path counts disagree with aggregate CSV in {bundle.manifest}: "
                f"{key}={manifest[key]} vs {aggregate[key]}")


def read_operations_csv(path):
    rows = []
    operations = set()
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != OPERATIONS_HEADER:
            raise ValueError(f"unexpected operations header in {path}: {reader.fieldnames!r}")
        for row in reader:
            operation = row["operation"]
            if operation in operations:
                raise ValueError(f"duplicate operation in {path}: {operation!r}")
            operations.add(operation)
            for key in OPERATIONS_NUMERIC_FIELDS:
                try:
                    row[key] = int(row[key])
                except (TypeError, ValueError) as exc:
                    raise ValueError(f"malformed numeric value in {path}: {row[key]!r}") from exc
            rows.append(row)
    return rows


def total_events(row):
    try:
        return int(row.get("total_events", 0))
    except (TypeError, ValueError) as exc:
        raise ValueError(f"malformed numeric total_events: {row.get('total_events')!r}") from exc


def has_ttl_events(row):
    try:
        return any(int(row.get(key, 0) or 0) != 0
                   for key in ("ttl_stores", "ttl_serves", "ttl_expires", "ttl_clears"))
    except (TypeError, ValueError) as exc:
        raise ValueError("malformed numeric TTL event field") from exc


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
    paths = set()
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != TOP_PATHS_HEADER:
            raise ValueError(f"unexpected top-paths header in {path}: {reader.fieldnames!r}")
        for row in reader:
            identity = (row["source"], row["path"])
            if identity in paths:
                raise ValueError(f"duplicate top-path in {path}: {identity!r}")
            paths.add(identity)
            for key in TOP_PATHS_NUMERIC_FIELDS:
                try:
                    row[key] = int(row[key])
                except (TypeError, ValueError) as exc:
                    raise ValueError(f"malformed numeric value in {path}: {row[key]!r}") from exc
            rows.append(row)
    return rows


def validate_bundle_consistency(bundle):
    aggregate = read_aggregate_csv(bundle.aggregate)
    validate_manifest_path_counts(bundle, aggregate)
    manifest = json.loads(bundle.manifest.read_text())
    operations = read_operations_csv(bundle.operations)
    top_paths = read_top_paths_csv(bundle.top_paths)

    def require(actual, expected, description):
        if actual != expected:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: {description}: "
                f"{actual} vs {expected}")

    operation_sums = defaultdict(int)
    source_sums = defaultdict(int)
    for row in operations:
        if any(row[key] < 0 for key in OPERATIONS_NUMERIC_FIELDS):
            raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: negative operation counter")
        latency_values = tuple(row[key] for key in
                               ("min_ns", "p50_ns", "p90_ns", "p95_ns", "p99_ns", "p999_ns", "max_ns"))
        if row["sampling_method"] != manifest["config"]["sampling_method"]:
            raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency sampling method")
        if row["sample_count"] > min(row["total_calls"], manifest["config"]["latency_capacity"]):
            raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency sample count")
        if row["total_calls"] == 0:
            if row["sample_count"] != 0 or row["cumulative_latency_ns"] != 0 or any(latency_values):
                raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency values for zero calls")
        else:
            minimum, *quantiles, maximum = latency_values
            if minimum > maximum:
                raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency minimum exceeds maximum")
            cumulative = row["cumulative_latency_ns"]
            if not row["total_calls"] * minimum <= cumulative <= row["total_calls"] * maximum:
                raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency cumulative outside exact bounds")
            if row["sample_count"] == 0:
                if any(quantiles):
                    raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency quantiles without samples")
            elif not all(left <= right for left, right in zip(latency_values, latency_values[1:])):
                raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: latency quantiles out of order")
        require(sum(row[key] for key in ("source_none", "source_metadata", "source_data", "source_tree", "source_bypass")),
                row["total_calls"], f"operation source total for {row['operation']!r}")
        if sum(row[key] for key in ("cache_hits", "cache_misses", "negative_results")) > row["total_calls"]:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: operation outcomes exceed calls "
                f"for {row['operation']!r}")
        for key in ("total_calls", "cumulative_latency_ns", "cache_hits", "cache_misses", "negative_results"):
            operation_sums[key] += row[key]
        for source in ("none", "metadata", "data", "tree", "bypass"):
            source_sums[source] += row[f"source_{source}"]

    aggregate_operation_fields = {
        "total_calls": "total_counted_fuse_operations",
        "cache_hits": "total_cache_hits",
        "cache_misses": "total_cache_misses",
        "negative_results": "total_negative_results",
        "cumulative_latency_ns": "cumulative_measured_latency_ns",
    }
    for operation_key, aggregate_key in aggregate_operation_fields.items():
        require(operation_sums[operation_key], aggregate[aggregate_key], aggregate_key)

    path_outcomes = defaultdict(int)
    path_ttl = defaultdict(int)
    path_sources = defaultdict(int)
    for row in top_paths:
        if any(row[key] < 0 for key in TOP_PATHS_NUMERIC_FIELDS):
            raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: negative top-path counter")
        event_parts = ("success", "cache_hits", "cache_misses", "negative_results",
                       "ttl_stores", "ttl_serves", "ttl_expires", "ttl_clears")
        require(sum(row[key] for key in event_parts), row["total_events"],
                f"top-path event total for {(row['source'], row['path'])!r}")
        outcome_count = sum(row[key] for key in ("success", "cache_hits", "cache_misses", "negative_results"))
        path_sources[row["source"]] += outcome_count
        for key in ("success", "cache_hits", "cache_misses", "negative_results"):
            path_outcomes[key] += row[key]
        for key in ("stores", "serves", "expires", "clears"):
            path_ttl[key] += row[f"ttl_{key}"]

    require(len(top_paths), aggregate["paths_retained"], "paths_retained")
    require(sum(row["total_events"] for row in top_paths) + aggregate["other_events"],
            aggregate["paths_seen"], "paths_seen")
    require(aggregate["paths_dropped"], aggregate["other_events"], "paths_dropped vs other_events")

    for key in ("stores", "serves", "expires", "clears"):
        aggregate_ttl = aggregate[f"md_enoent_ttl_{key}"]
        if path_ttl[key] > aggregate_ttl:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: retained ttl_{key} exceeds aggregate: "
                f"{path_ttl[key]} vs {aggregate_ttl}")

    operation_outcomes = {
        "cache_hits": operation_sums["cache_hits"],
        "cache_misses": operation_sums["cache_misses"],
        "negative_results": operation_sums["negative_results"],
    }
    operation_outcomes["success"] = operation_sums["total_calls"] - sum(operation_outcomes.values())
    for key, operation_count in operation_outcomes.items():
        if path_outcomes[key] > operation_count:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: retained path {key} "
                f"exceeds operation total: {path_outcomes[key]} vs {operation_count}")
    for source, operation_count in source_sums.items():
        if path_sources[source] > operation_count:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: retained path source {source} "
                f"exceeds operation total: {path_sources[source]} vs {operation_count}")

    # Empty-path operations are intentionally absent from the top-path table.
    # Source-none is how production pathless callbacks are recorded, so their
    # outcomes cannot be subtracted per outcome. Non-none source totals remain
    # exact when nothing dropped, as do all totals when no source-none calls exist.
    if aggregate["other_events"] == 0:
        if source_sums["none"] == 0:
            for key, expected in operation_outcomes.items():
                require(path_outcomes[key], expected, f"complete path {key}")
        for source, expected in source_sums.items():
            if source != "none":
                require(path_sources[source], expected, f"complete path source {source}")
        for key in ("stores", "serves", "expires", "clears"):
            require(path_ttl[key], aggregate[f"md_enoent_ttl_{key}"], f"md_enoent_ttl_{key}")

    return aggregate, operations, top_paths


def is_final_rank_file(path):
    name = os.path.basename(path)
    legacy = r"^.+-\d+-profiling_(aggregate|operations|top_paths)\.csv$"
    bundled = r"^.+-\d+-\d+-final-profiling_(aggregate|operations|top_paths)\.csv$"
    return re.match(legacy, name) is not None or re.match(bundled, name) is not None


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
    try:
        bundles = discover_committed_bundles(search_dir)
        validated_bundles = [(bundle, *validate_bundle_consistency(bundle)) for bundle in bundles]
    except ValueError as exc:
        print(f"invalid profiling bundles: {exc}", file=sys.stderr)
        return 1
    aggregate_files = [str(bundle.aggregate) for bundle, _, _, _ in validated_bundles]
    operations_files = [str(bundle.operations) for bundle, _, _, _ in validated_bundles]
    top_paths_files = [str(bundle.top_paths) for bundle, _, _, _ in validated_bundles]

    if not aggregate_files or not operations_files:
        print(f"failed to find profiling csv files under: {search_dir}")
        return 1

    os.makedirs(profiling_cluster_dir, exist_ok=True)

    aggregate_totals = defaultdict(int)
    operation_totals = defaultdict(lambda: defaultdict(int))
    path_class_totals = defaultdict(lambda: {"entries": 0, "total_events": 0, "example": ""})
    metadata_ttl_serves = 0
    missing_shared_lib_probe_events = 0
    python_probe_events = 0
    observed_paths = read_observed_paths(tables_final_dir)

    for _, aggregate, _, _ in validated_bundles:
        for key, value in aggregate.items():
            aggregate_totals[key] += value

    for _, _, operations, _ in validated_bundles:
        for row in operations:
            op = row["operation"]
            for key in ["total_calls", "cache_hits", "cache_misses", "negative_results", "cumulative_latency_ns"]:
                operation_totals[op][key] += row[key]

    for _, _, _, top_paths in validated_bundles:
        for row in top_paths:
            if "source" in row and "category" not in row:
                row["category"] = row["source"]
            if has_ttl_events(row):
                row["category"] = "metadata_ttl"
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
        writer.writerow(["operation", "total_calls", "cache_hits", "cache_misses", "negative_results", "avg_latency_ns", "cumulative_latency_ns"])
        for op in sorted(operation_totals.keys()):
            total_calls = operation_totals[op]["total_calls"]
            cumulative_latency_ns = operation_totals[op]["cumulative_latency_ns"]
            avg_latency_ns = (cumulative_latency_ns / total_calls) if total_calls else 0.0
            writer.writerow([
                op,
                operation_totals[op]["total_calls"],
                operation_totals[op]["cache_hits"],
                operation_totals[op]["cache_misses"],
                operation_totals[op]["negative_results"],
                f"{avg_latency_ns:.2f}",
                cumulative_latency_ns,
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
            f.write(f"| `{key}` | {aggregate_totals[key]} |\n")

        f.write("\n## Operation Totals\n\n")
        f.write("| Operation | Total Calls | Cache Hits | Cache Misses | Negative Results | Avg Latency (ns) | Cumulative Latency (ns) |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|\n")
        for op in sorted(operation_totals.keys()):
            total_calls = operation_totals[op]["total_calls"]
            cumulative_latency_ns = operation_totals[op]["cumulative_latency_ns"]
            avg_latency_ns = (cumulative_latency_ns / total_calls) if total_calls else 0.0
            f.write(
                f"| `{op}` | {total_calls} | {operation_totals[op]['cache_hits']} | "
                f"{operation_totals[op]['cache_misses']} | {operation_totals[op]['negative_results']} | "
                f"{avg_latency_ns:.2f} | {cumulative_latency_ns} |\n"
            )

        if path_class_totals:
            sorted_classes = sorted(path_class_totals.items(), key=lambda item: item[1]["total_events"], reverse=True)
            f.write("\n## Path Class Heuristics\n\n")
            f.write("This section groups the bounded admitted paths into coarse classes so users can reason about what to prune and what to keep. In bounded mode these are the first distinct paths admitted, not a hottest-path ranking.\n\n")
            f.write("| Path Class | Entries | Total Events | Example |\n")
            f.write("|---|---:|---:|---|\n")
            for path_class, stats in sorted_classes:
                f.write(f"| `{path_class}` | {stats['entries']} | {stats['total_events']} | `{stats['example']}` |\n")

            f.write("\n## Pruning Guidance\n\n")
            f.write("These are heuristic suggestions derived from the bounded admitted paths and TTL paths. They are intended to guide a staged cleanup, not to justify deleting unseen files from the original environment.\n\n")
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
