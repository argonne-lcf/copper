#!/usr/bin/env python3

import argparse
import csv
import glob
import json
import os
import pathlib
import re
import sys
import io
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from copper_report import publish_generation, summarize_operations, histogram_bounds, parse_errors, join_receipts
import hashlib
from collections import defaultdict
from collections import namedtuple

SUPPORTED_MANIFEST_SCHEMAS = {2, 3}
SUPPORTED_PROFILING_MODES = {"aggregate-only", "bounded-admitted-set", "bounded-heavy-hitter", "full-path"}
SUPPORTED_SAMPLING_METHODS = {"reservoir_algorithm_r"}
AGGREGATE_HEADER = ["metric", "value"]
OPERATIONS_HEADER = ["operation", "total_calls", "cumulative_latency_ns", "min_ns", "p50_ns", "p90_ns", "p95_ns", "p99_ns", "p999_ns", "max_ns", "sample_count", "sampling_method", "cache_hits", "cache_misses", "negative_results", "source_none", "source_metadata", "source_data", "source_tree", "source_bypass", "errors"]
OPERATIONS_NUMERIC_FIELDS = [field for field in OPERATIONS_HEADER if field not in {"operation", "sampling_method", "errors"}]
TOP_PATHS_HEADER = ["source", "path", "total_events", "success", "cache_hits", "cache_misses", "negative_results", "ttl_stores", "ttl_serves", "ttl_expires", "ttl_clears", "errors"]
TOP_PATHS_NUMERIC_FIELDS = [field for field in TOP_PATHS_HEADER if field not in {"source", "path", "errors"}]
AGGREGATE_METRICS = {"total_counted_fuse_operations", "total_cache_hits", "total_cache_misses", "total_negative_results", "cumulative_measured_latency_ns", "paths_seen", "paths_retained", "paths_dropped", "other_events", "md_enoent_ttl_stores", "md_enoent_ttl_serves", "md_enoent_ttl_expires", "md_enoent_ttl_clears"}

def filename_component(value):
    """Match ProfilerCore::filename_component for manifest identity checks."""
    sanitized = "".join(c if ord(c) < 128 and (c.isalnum() or c in "-_.") else "_"
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

HISTOGRAM_CONTRACT = {"method": "fixed_log2_integer", "sample_provenance": "all_completions", "bucket_count": 65, "bounds": "inclusive", "unit": "ns"}
UINT64_MAX = 2**64 - 1

def valid_manifest(data):
    if data.get("schema_version") == 2:
        return valid_v2_manifest(data)
    extras = {"run_id", "daemon_generation", "semantics", "reset_epoch", "histogram", "file_integrity"}
    base = {k: v for k, v in data.items() if k not in extras}
    if not valid_v2_manifest(base) or not extras <= data.keys():
        return False
    return (all(data[k] is None or isinstance(data[k], str) and bool(data[k]) for k in ("run_id", "daemon_generation"))
            and data["semantics"] in {"cumulative", "interval"}
            and type(data["reset_epoch"]) is int and 0 <= data["reset_epoch"] <= UINT64_MAX
            and data["histogram"] == HISTOGRAM_CONTRACT
            and isinstance(data["file_integrity"], dict))

def verify_integrity(bundle, data):
    if bundle.schema_version != 3:
        return
    entries = data["file_integrity"]
    if set(entries) != {p.name for p in bundle.files}:
        raise ValueError("file integrity inventory mismatch")
    for path in bundle.files:
        item = entries[path.name]
        if (not isinstance(item, dict) or set(item) != {"size_bytes", "sha256"}
                or type(item["size_bytes"]) is not int or item["size_bytes"] < 0
                or not isinstance(item["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"])):
            raise ValueError("invalid file integrity metadata")
        raw = path.read_bytes()
        if len(raw) != item["size_bytes"] or hashlib.sha256(raw).hexdigest() != item["sha256"]:
            raise ValueError(f"checksum/size integrity mismatch: {path}")

def read_histogram_csv(path, operations):
    result = defaultdict(dict)
    with open(path, newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["operation", "bucket_index", "lower_bound_ns", "upper_bound_ns", "count"]:
            raise ValueError("unexpected histogram header")
        for row in reader:
            if None in row or any(v is None for v in row.values()):
                raise ValueError("CSV row shape mismatch")
            try:
                index, lower, upper, count = (int(row[k]) for k in ("bucket_index", "lower_bound_ns", "upper_bound_ns", "count"))
            except (ValueError, TypeError) as exc:
                raise ValueError("malformed histogram numeric value") from exc
            if not 0 <= index < 65 or (lower, upper) != histogram_bounds(index) or not 0 <= count <= UINT64_MAX:
                raise ValueError("invalid histogram bounds/count")
            if index in result[row["operation"]]:
                raise ValueError("duplicate histogram bucket")
            result[row["operation"]][index] = count
    expected = {r["operation"] for r in operations if r["total_calls"]}
    if set(result) != expected:
        raise ValueError("histogram operation coverage mismatch")
    output = {}
    for row in operations:
        op = row["operation"]
        if not row["total_calls"]:
            continue
        if set(result[op]) != set(range(65)) or sum(result[op].values()) != row["total_calls"]:
            raise ValueError("histogram bucket coverage/count mismatch")
        counts = [result[op][i] for i in range(65)]
        if not sum(c * histogram_bounds(i)[0] for i, c in enumerate(counts)) <= row["cumulative_latency_ns"] <= sum(c * histogram_bounds(i)[1] for i, c in enumerate(counts)):
            raise ValueError("histogram cumulative bounds mismatch")
        output[op] = counts
    return output

Bundle = namedtuple("Bundle", "schema_version manifest files aggregate operations top_paths histogram")

def discover_committed_bundles(directory):
    root = pathlib.Path(directory)
    candidates = []
    candidate_identities = {}
    logical_owners = defaultdict(set)
    schemas = set()
    for manifest in sorted(root.glob("*-manifest.json")):
        schema = None
        try:
            data = json.loads(manifest.read_text())
            schema = data["schema_version"]
            if schema == 3 and type(schema) is int and not valid_manifest(data):
                raise ValueError("invalid schema3 manifest")
            if type(schema) is not int or schema not in SUPPORTED_MANIFEST_SCHEMAS or not valid_manifest(data):
                continue
            names = data["files"]
            stem_suffix = "-manifest.json"
            stem = manifest.name[:-len(stem_suffix)]
            expected = [f"{stem}-profiling_{kind}.csv" for kind in (("aggregate", "operations", "top_paths", "histogram") if schema == 3 else ("aggregate", "operations", "top_paths"))]
            if (not isinstance(names, list) or len(names) != len(expected) or
                    len(set(names)) != len(names) or set(names) != set(expected) or
                    any(not isinstance(n, str) or pathlib.Path(n).name != n for n in names)):
                if schema == 3:
                    raise ValueError("invalid schema3 file inventory")
                continue
            files = [root / n for n in expected]
            aggregate, operations, top_paths = files[:3]
            if not all(p.is_file() for p in files):
                if schema == 3:
                    raise ValueError("missing schema3 committed file")
                continue
            logical_identity = (data["host"], data["pid"], data["sequence"], data["kind"])
            logical_owners[logical_identity].add(manifest)
            expected_stem = (f"{filename_component(data['host'])}-{data['pid']}-"
                             f"{data['sequence']}-{filename_component(data['kind'])}")
            if stem != expected_stem:
                if schema == 3:
                    raise ValueError("schema3 manifest identity mismatch")
                continue
        except (OSError, ValueError, TypeError, KeyError, StopIteration, json.JSONDecodeError) as exc:
            if schema == 3:
                raise ValueError(f"invalid schema3 commit {manifest}: {exc}") from exc
            continue
        bundle = Bundle(schema, manifest, files, aggregate, operations, top_paths, files[3] if schema == 3 else None)
        verify_integrity(bundle, data)
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
    if any(b.schema_version == 3 for b in candidates if b not in bundles):
        raise ValueError("duplicate schema3 bundle identity or shared file")
    schemas.update(bundle.schema_version for bundle in bundles)
    if len(schemas) > 1:
        raise ValueError("mixed schema versions in committed profiling bundles")
    return bundles


def select_schema3(bundles, expected_hosts, allow_partial, run_id, generation):
    metadata = [(b, json.loads(b.manifest.read_text())) for b in bundles]
    if run_id is not None:
        metadata = [(b, d) for b, d in metadata if d["run_id"] == run_id]
    if generation is not None:
        metadata = [(b, d) for b, d in metadata if d["daemon_generation"] == generation]
    identities = {(d["run_id"], d["daemon_generation"]) for _, d in metadata}
    if len(identities) != 1 or any(None in identity for identity in identities):
        raise ValueError("ambiguous or unknown run/generation identity; select --run-id and --daemon-generation")
    run, gen = next(iter(identities))
    observed = {d["host"] for _, d in metadata}
    missing = sorted(set(expected_hosts) - observed)
    if observed - set(expected_hosts):
        raise ValueError("unexpected hosts")
    if missing and not allow_partial:
        raise ValueError("missing expected hosts")
    selected = []
    final = True
    gaps = []
    for host in sorted(observed):
        rows = sorted(((b, d) for b, d in metadata if d["host"] == host), key=lambda item: item[1]["sequence"])
        if len({d["pid"] for _, d in rows}) != 1 or len({d["sequence"] for _, d in rows}) != len(rows):
            raise ValueError("ambiguous restart/process identity or sequence")
        epochs = [d["reset_epoch"] for _, d in rows]
        if epochs != sorted(epochs):
            raise ValueError("reset epoch regressed")
        groups = defaultdict(list)
        for bundle, data in rows:
            groups[data["reset_epoch"]].append((bundle, data))
        last_epoch = max(groups)
        if set(groups) != set(range(last_epoch + 1)):
            gaps.append(host)
        for epoch, snapshots in sorted(groups.items()):
            previous = None
            previous_histograms = None
            for bundle, data in snapshots:
                current = {r["operation"]: r for r in read_operations_csv(bundle.operations)}
                if previous is not None:
                    for op, old in previous.items():
                        new = current.get(op)
                        if new is None or any(new[k] < old[k] for k in OPERATIONS_NUMERIC_FIELDS if k not in {"min_ns", "max_ns", "p50_ns", "p90_ns", "p95_ns", "p99_ns", "p999_ns"}):
                            raise ValueError("cumulative operation counters regressed")
                        old_errors, new_errors = parse_errors(old["errors"]), parse_errors(new["errors"])
                        if any(new_errors.get(k, 0) < v for k, v in old_errors.items()):
                            raise ValueError("cumulative error counters regressed")
                histograms = read_histogram_csv(bundle.histogram, list(current.values()))
                if previous_histograms is not None:
                    for op, counts in previous_histograms.items():
                        if op not in histograms or any(a > b for a, b in zip(counts, histograms[op])):
                            raise ValueError("cumulative histogram buckets regressed")
                previous_histograms = histograms
                previous = current
            for bundle, _ in snapshots:
                validate_bundle_consistency(bundle)
            closed = [(b, d) for b, d in snapshots if d["semantics"] == "interval"]
            if len(closed) > 1 or closed and closed[0] != snapshots[-1]:
                raise ValueError("overlapping interval/cumulative epoch snapshots")
            if epoch != last_epoch and not closed:
                gaps.append(host)
            selected.append(snapshots[-1][0])
        final = final and rows[-1][1]["kind"] == "final"
    if (gaps or not final) and not allow_partial:
        raise ValueError("non-final or missing closed reset epochs require --allow-partial")
    coverage = {"expected_hosts": sorted(expected_hosts), "observed_hosts": sorted(observed),
                "missing_hosts": missing, "unexpected_hosts": [], "run_id": run,
                "daemon_generation": gen, "epoch_gaps": sorted(set(gaps)),
                "complete": not missing and not gaps and final,
                "roster_verified": True, "final": final,
                "discarded_snapshots": [str(b.manifest) for b in bundles if b not in selected]}
    return selected, coverage


def select_bundles(bundles, expected_hosts, allow_partial=False, legacy_unverified=False, run_id=None, generation=None):
    if not expected_hosts or len(set(expected_hosts)) != len(expected_hosts):
        raise ValueError("exact expected host roster is required (no duplicates)")
    if bundles and all(b.schema_version == 3 for b in bundles):
        return select_schema3(bundles, expected_hosts, allow_partial, run_id, generation)
    metadata = [(bundle, json.loads(bundle.manifest.read_text())) for bundle in bundles]
    if any(data["schema_version"] == 2 for _, data in metadata) and not legacy_unverified:
        raise ValueError("schema2 requires explicit --legacy-unverified")
    observed = {data["host"] for _, data in metadata}
    unexpected = sorted(observed - set(expected_hosts))
    missing = sorted(set(expected_hosts) - observed)
    if unexpected:
        raise ValueError(f"unexpected hosts: {unexpected}")
    if missing and not allow_partial:
        raise ValueError(f"missing expected hosts: {missing}")
    selected = []
    for host in sorted(observed):
        candidates = [(bundle, data) for bundle, data in metadata if data["host"] == host]
        latest_sequence = max(data["sequence"] for _, data in candidates)
        latest = [(bundle, data) for bundle, data in candidates if data["sequence"] == latest_sequence]
        if len(latest) != 1:
            raise ValueError(f"ambiguous restart snapshot for {host}")
        selected.append(latest[0][0])
    final = all(json.loads(bundle.manifest.read_text())["kind"] == "final" for bundle in selected)
    if not final and not allow_partial:
        raise ValueError("non-final recovered snapshots require --allow-partial")
    coverage = {"expected_hosts": sorted(expected_hosts), "observed_hosts": sorted(observed),
                "missing_hosts": missing, "unexpected_hosts": unexpected,
                "complete": False, "roster_verified": not legacy_unverified, "final": final,
                "discarded_snapshots": [str(b.manifest) for b in bundles if b not in selected]}
    return selected, coverage


def usage():
    print("usage: aggregate_profiling.py <log_dir> [output_prefix]")


def read_aggregate_csv(path):
    data = {}
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != AGGREGATE_HEADER:
            raise ValueError(f"unexpected aggregate header in {path}: {reader.fieldnames!r}")
        for row in reader:
            if None in row or any(v is None for v in row.values()):
                raise ValueError("CSV row shape mismatch")
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
            if None in row or any(v is None for v in row.values()):
                raise ValueError("CSV row shape mismatch")
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


def read_top_paths_csv(path, schema=2):
    rows = []
    paths = set()
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        header = TOP_PATHS_HEADER + (["estimated_total", "error_bound"] if schema == 3 else [])
        if reader.fieldnames != header:
            raise ValueError(f"unexpected top-paths header in {path}: {reader.fieldnames!r}")
        for row in reader:
            if None in row or any(v is None for v in row.values()):
                raise ValueError("CSV row shape mismatch")
            identity = (row["source"], row["path"])
            if identity in paths:
                raise ValueError(f"duplicate top-path in {path}: {identity!r}")
            paths.add(identity)
            for key in TOP_PATHS_NUMERIC_FIELDS + (["estimated_total", "error_bound"] if schema == 3 else []):
                try:
                    row[key] = int(row[key])
                except (TypeError, ValueError) as exc:
                    raise ValueError(f"malformed numeric value in {path}: {row[key]!r}") from exc
            rows.append(row)
    return rows


def validate_bundle_consistency(bundle):
    verify_integrity(bundle, json.loads(bundle.manifest.read_text()))
    aggregate = read_aggregate_csv(bundle.aggregate)
    validate_manifest_path_counts(bundle, aggregate)
    manifest = json.loads(bundle.manifest.read_text())
    operations = read_operations_csv(bundle.operations)
    top_paths = read_top_paths_csv(bundle.top_paths, bundle.schema_version)
    if bundle.histogram:
        histograms = read_histogram_csv(bundle.histogram, operations)
        for row in operations:
            row["histogram"] = histograms.get(row["operation"], [0] * 65)

    def require(actual, expected, description):
        if actual != expected:
            raise ValueError(
                f"inconsistent profiling bundle {bundle.manifest}: {description}: "
                f"{actual} vs {expected}")

    if bundle.schema_version == 3 and any(not 0 <= v <= UINT64_MAX for v in aggregate.values()):
        raise ValueError("aggregate counter outside uint64")
    operation_sums = defaultdict(int)
    source_sums = defaultdict(int)
    for row in operations:
        if bundle.schema_version == 3 and any(row[k] > UINT64_MAX for k in OPERATIONS_NUMERIC_FIELDS):
            raise ValueError("operation counter outside uint64")
        if sum(parse_errors(row["errors"]).values()) > row["total_calls"]:
            raise ValueError("operation error counts exceed calls")
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
        if sum(parse_errors(row["errors"]).values()) > row["total_events"]:
            raise ValueError("path error counts exceed events")
        if bundle.schema_version == 3 and any(row[k] > UINT64_MAX for k in TOP_PATHS_NUMERIC_FIELDS + ["estimated_total", "error_bound"]):
            raise ValueError("path counter outside uint64")
        if any(row[key] < 0 for key in TOP_PATHS_NUMERIC_FIELDS):
            raise ValueError(f"inconsistent profiling bundle {bundle.manifest}: negative top-path counter")
        if bundle.schema_version == 3:
            estimate, error = row["estimated_total"], row["error_bound"]
            if not 0 <= error <= estimate <= aggregate["paths_seen"] or estimate - error != row["total_events"]:
                raise ValueError("inconsistent path estimate/error bound")
            if manifest["mode"] != "bounded-heavy-hitter" and (error or estimate != row["total_events"]):
                raise ValueError("exact path mode has approximate counts")
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


def main(argv=None):
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("log_dir")
    parser.add_argument("output_prefix", nargs="?", default="cluster")
    parser.add_argument("--expected-host", action="append", default=[])
    parser.add_argument("--allow-partial", action="store_true")
    parser.add_argument("--legacy-unverified", action="store_true")
    parser.add_argument("--run-id")
    parser.add_argument("--daemon-generation")
    parser.add_argument("--output-dir")
    for kind in ("startup", "workload", "cleanup", "cache"):
        parser.add_argument("--" + kind + "-receipt")
    parser.add_argument("--usage-root", action="append", default=[],
                        help="additional filesystem root to analyze for observed-path coverage; can be passed multiple times")
    args = parser.parse_args(argv)

    log_dir = os.path.abspath(args.log_dir)
    output_prefix = args.output_prefix
    profiling_final_dir = os.path.join(log_dir, "profiling", "final")
    profiling_cluster_dir = args.output_dir or os.path.join(log_dir, "profiling", "cluster")
    tables_final_dir = os.path.join(log_dir, "tables", "final")

    search_dir = profiling_final_dir if os.path.isdir(profiling_final_dir) else log_dir
    try:
        bundles = discover_committed_bundles(search_dir)
        bundles, coverage = select_bundles(bundles, args.expected_host, args.allow_partial, args.legacy_unverified, args.run_id, args.daemon_generation)
        validated_bundles = [(bundle, *validate_bundle_consistency(bundle)) for bundle in bundles]
    except (ValueError, OSError) as exc:
        print(f"invalid profiling bundles: {exc}", file=sys.stderr)
        return 1
    aggregate_files = [str(bundle.aggregate) for bundle, _, _, _ in validated_bundles]
    operations_files = [str(bundle.operations) for bundle, _, _, _ in validated_bundles]
    top_paths_files = [str(bundle.top_paths) for bundle, _, _, _ in validated_bundles]

    if not aggregate_files or not operations_files:
        print(f"failed to find profiling csv files under: {search_dir}")
        return 1

    rendered = {}

    class RenderBuffer(io.StringIO):
        def __init__(self, name):
            super().__init__(); self.name = name
        def __exit__(self, *args):
            rendered[pathlib.Path(self.name).name] = self.getvalue().encode()
            return super().__exit__(*args)

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
    with RenderBuffer(aggregate_csv_path) as f:
        writer = csv.writer(f)
        writer.writerow(["metric", "value"])
        writer.writerow(["ranks_with_aggregate_files", len(aggregate_files)])
        for key in sorted(aggregate_totals.keys()):
            writer.writerow([key, aggregate_totals[key]])

    operations_csv_path = os.path.join(profiling_cluster_dir, f"{output_prefix}-profiling_operations.csv")
    with RenderBuffer(operations_csv_path) as f:
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
    with RenderBuffer(summary_md_path) as f:
        f.write("# Cluster Profiling Summary\n\n")
        f.write(f"- Coverage: {json.dumps(coverage, sort_keys=True)}\n")
        f.write("- Cluster percentiles use merged all-completion histogram bucket bounds where available; reservoir quantiles are never averaged.\n\n")
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

    try:
        report = summarize_operations([(json.loads(bundle.manifest.read_text())["host"], operations)
                                      for bundle, _, operations, _ in validated_bundles])
    except ValueError as exc:
        print(f"invalid operation totals: {exc}", file=sys.stderr)
        return 1
    manifests = [json.loads(b.manifest.read_text()) for b, _, _, _ in validated_bundles]
    compatible = [{k: d.get(k) for k in ("schema_version", "mode", "config", "histogram")} for d in manifests]
    if any(c != compatible[0] for c in compatible):
        print("incompatible profiling configurations", file=sys.stderr)
        return 1
    report.update(report_schema_version=1, run_id=coverage.get("run_id"),
                  daemon_generation=coverage.get("daemon_generation"), compatibility=compatible[0],
                  receipts=join_receipts(coverage.get("run_id"), {k: getattr(args, k+"_receipt") for k in ("startup", "workload", "cleanup", "cache") if getattr(args, k+"_receipt")}),
                  paths=[dict(row, host=json.loads(b.manifest.read_text())["host"], reset_epoch=json.loads(b.manifest.read_text()).get("reset_epoch"), manifest=str(b.manifest)) for b, _, _, rows in validated_bundles for row in rows],
                  provenance=[dict(d, manifest=str(b.manifest), manifest_sha256=hashlib.sha256(b.manifest.read_bytes()).hexdigest()) for (b, _, _, _), d in zip(validated_bundles, manifests)],
                  coverage=coverage, aggregate=dict(aggregate_totals),
                  selected_manifests=[str(bundle.manifest) for bundle, _, _, _ in validated_bundles])
    if any("histogram" in row for row in report["operations"]):
        with RenderBuffer(f"{output_prefix}-profiling_histogram.csv") as stream:
            writer = csv.writer(stream)
            writer.writerow(["operation", "bucket_index", "lower_bound_ns", "upper_bound_ns", "count"])
            for row in report["operations"]:
                for i, count in enumerate(row["histogram"]):
                    writer.writerow([row["operation"], i, *histogram_bounds(i), count])
        human = "\n## Cluster Histogram Percentile Bounds\n\nAll-completion inclusive nanosecond bounds; logarithmic discretization, not reservoir sampling.\n\n| Operation | Quantile | Lower ns | Upper ns |\n|---|---|---:|---:|\n"
        hosts = "\n## Host Percentile Distribution\n\nDistribution of per-host bounds, NOT cluster quantiles.\n\n| Operation | Quantile | Hosts | Min upper ns | Median upper ns | Max upper ns |\n|---|---|---:|---:|---:|---:|\n"
        for row in report["operations"]:
            for name, bounds in row["cluster_percentiles"].items():
                human += f"| `{row['operation']}` | {name} | {bounds['lower_bound_ns']} | {bounds['upper_bound_ns']} |\n"
                distribution = row["host_percentile_distribution"][name]
                hosts += f"| `{row['operation']}` | {name} | {distribution['host_count']} | {distribution['min_upper_bound_ns']} | {distribution['median_upper_bound_ns']} | {distribution['max_upper_bound_ns']} |\n"
        rendered[f"{output_prefix}-profiling_summary.md"] += (human + hosts).encode()
    rendered[f"{output_prefix}-report.json"] = json.dumps(report, sort_keys=True, indent=2).encode()
    try:
        publish_generation(profiling_cluster_dir, output_prefix, rendered)
    except (OSError, ValueError) as exc:
        print(f"report publication failed: {exc}", file=sys.stderr)
        return 1
    print(f"wrote {aggregate_csv_path}")
    print(f"wrote {operations_csv_path}")
    print(f"wrote {summary_md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
