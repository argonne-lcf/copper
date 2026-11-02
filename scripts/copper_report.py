#!/usr/bin/env python3
"""Offline, provenance-preserving COPPER profiling report support."""
from collections import defaultdict
import hashlib
import json
import os
import pathlib
import shutil
import tempfile
import uuid


def main(argv=None):
    from aggregate_profiling import main as aggregate_main
    return aggregate_main(argv)


def load_report(path):
    """Pin a generation once and verify its committed inventory."""
    path = pathlib.Path(path).resolve()
    if path.is_dir():
        manifest = json.loads((path / "manifest.json").read_text())
        if manifest.get("schema_version") != 1 or manifest.get("checksum_algorithm") != "sha256":
            raise ValueError("unsupported report generation")
        files = manifest["files"]
        if len(set(files)) != len(files) or set(files) != set(manifest["checksums"]):
            raise ValueError("generation inventory mismatch")
        for name in files:
            if pathlib.Path(name).name != name:
                raise ValueError("unsafe generation inventory")
            if hashlib.sha256((path / name).read_bytes()).hexdigest() != manifest["checksums"][name]:
                raise ValueError("report generation checksum mismatch")
        reports = [name for name in files if name.endswith("-report.json")]
        if len(reports) != 1:
            raise ValueError("ambiguous report inventory")
        path = path / reports[0]
    report = json.loads(path.read_text())
    if report.get("report_schema_version") != 1:
        raise ValueError("unsupported report schema")
    return report


def compare_reports(baseline, candidate):
    mismatches = []
    for key in ("schema_version", "mode", "config", "histogram"):
        left, right = baseline.get("compatibility", {}).get(key), candidate.get("compatibility", {}).get(key)
        if left != right or left is None and key != "histogram":
            mismatches.append({"field":key, "baseline":left, "candidate":right})
    for key in ("expected_hosts", "observed_hosts"):
        left, right = baseline.get("coverage", {}).get(key), candidate.get("coverage", {}).get(key)
        if left != right or left is None:
            mismatches.append({"field":"coverage."+key, "baseline":left, "candidate":right})
    for side, data in (("baseline",baseline), ("candidate",candidate)):
        if not data.get("coverage", {}).get("complete"):
            mismatches.append({"field":side+".coverage.complete", "reason":"unverified or incomplete run"})
    result = {"compatible":not mismatches, "mismatches":mismatches,
              "baseline_run_id":baseline.get("run_id"), "candidate_run_id":candidate.get("run_id"), "operations":[]}
    if mismatches:
        return result
    left = {r["operation"]:r for r in baseline["operations"]}
    right = {r["operation"]:r for r in candidate["operations"]}
    for op in sorted(set(left) | set(right)):
        item = {"operation":op}
        for key in COUNTERS:
            a, b = left.get(op, {}).get(key,0), right.get(op, {}).get(key,0)
            item[key] = {"baseline":a, "candidate":b, "delta":b-a, "ratio":b/a if a else None}
        for key in ("errors", "cluster_percentiles", "host_percentile_distribution"):
            item[key] = {"baseline":left.get(op,{}).get(key), "candidate":right.get(op,{}).get(key)}
        result["operations"].append(item)
    return result


def compare_main(argv=None):
    import argparse
    import sys
    parser = argparse.ArgumentParser(description="Compare verified compatible COPPER run reports")
    parser.add_argument("baseline")
    parser.add_argument("candidate")
    parser.add_argument("--output-dir")
    parser.add_argument("--prefix", default="comparison")
    args = parser.parse_args(argv)
    try:
        result = compare_reports(load_report(args.baseline), load_report(args.candidate))
        rendered = json.dumps(result, sort_keys=True, indent=2).encode()
        if args.output_dir:
            publish_generation(args.output_dir, args.prefix, {args.prefix+"-comparison.json":rendered})
        print(rendered.decode())
        return 0 if result["compatible"] else 2
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(f"comparison failed: {exc}", file=sys.stderr)
        return 1


def histogram_bounds(index):
    if not 0 <= index <= 64:
        raise ValueError("invalid histogram bucket index")
    return (0, 0) if index == 0 else (2**(index-1), 2**index-1)


def publish_generation(root, prefix, outputs):
    """Commit an immutable inventory, then switch exactly one stable symlink.

    Readers requiring multi-file consistency pin current.resolve() once.
    No generation is deleted: interrupted readers keep their exact inventory.
    """
    root = pathlib.Path(root)
    if not prefix or pathlib.Path(prefix).name != prefix or prefix in {".", ".."}:
        raise ValueError("unsafe output prefix")
    if not outputs or any(pathlib.Path(n).name != n or n == "manifest.json" for n in outputs):
        raise ValueError("unsafe or empty output inventory")
    root.mkdir(parents=True, exist_ok=True)
    generations = root / "generations"
    generations.mkdir(exist_ok=True)
    identifier = prefix + "-" + uuid.uuid4().hex
    stage = pathlib.Path(tempfile.mkdtemp(prefix=".staging-", dir=generations))
    destination = generations / identifier
    current = root / (prefix + "-current")
    link = root / (".current-" + uuid.uuid4().hex)
    try:
        checksums = {}
        for name, content in outputs.items():
            with (stage / name).open("wb") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            checksums[name] = hashlib.sha256(content).hexdigest()
        manifest = {"schema_version": 1, "generation": identifier,
                    "files": sorted(outputs), "checksum_algorithm": "sha256", "checksums": checksums}
        with (stage / "manifest.json").open("w") as stream:
            json.dump(manifest, stream, sort_keys=True)
            stream.flush()
            os.fsync(stream.fileno())
        for name, checksum in checksums.items():
            if hashlib.sha256((stage / name).read_bytes()).hexdigest() != checksum:
                raise ValueError("staged checksum mismatch")
        os.replace(stage, destination)
        # Compatibility names always dereference the one commit pointer.
        for name in outputs:
            alias = root / name
            target = str(pathlib.Path(prefix + "-current") / name)
            if alias.is_symlink() and os.readlink(alias) == target:
                continue
            if alias.exists() or alias.is_symlink():
                raise ValueError(f"refusing to replace unmanaged report file: {alias}")
            alias.symlink_to(target)
        link.symlink_to(destination.relative_to(root))
        os.replace(link, current)
        return destination.resolve()
    finally:
        if stage.exists():
            shutil.rmtree(stage)
        if link.is_symlink():
            link.unlink()


def join_receipts(run_id, paths):
    result = {}
    for kind in ("startup", "workload", "cleanup", "cache"):
        entry = {"status": "unavailable", "data": None, "source": None,
                 "reason": "receipt not supplied"}
        if kind in paths:
            path = pathlib.Path(paths[kind])
            entry["source"] = str(path)
            try:
                raw = path.read_bytes()
                data = json.loads(raw)
                if not isinstance(data, dict):
                    raise ValueError("receipt must be a JSON object")
                entry["sha256"] = hashlib.sha256(raw).hexdigest()
                if not run_id or data.get("run_id") != run_id:
                    entry.update(status="mismatch", reason="receipt run identity absent or mismatched")
                else:
                    entry.update(status="available", data=data, reason=None)
            except (OSError, ValueError) as exc:
                entry["reason"] = str(exc)
        result[kind] = entry
    return result


COUNTERS = ("total_calls", "cumulative_latency_ns", "cache_hits", "cache_misses",
            "negative_results", "source_none", "source_metadata", "source_data",
            "source_tree", "source_bypass", "sample_count")


def parse_errors(text):
    result = {}
    for entry in text.split(";") if text else []:
        code, count = entry.split(":")
        if str(int(code)) != code or int(count) < 0 or code in result:
            raise ValueError("malformed error breakdown")
        result[code] = int(count)
    return result


QUANTILES = {"p50": (1,2), "p90": (9,10), "p95": (95,100), "p99": (99,100), "p999": (999,1000)}


def histogram_quantiles(counts):
    if len(counts) != 65 or any(type(c) is not int or c < 0 for c in counts):
        raise ValueError("invalid histogram counts")
    total = sum(counts)
    if total > 2**64 - 1:
        raise ValueError("histogram count overflow")
    result = {}
    for name, (numerator, denominator) in QUANTILES.items():
        rank = (total * numerator + denominator - 1) // denominator
        cumulative = 0
        index = 0
        for index, count in enumerate(counts):
            cumulative += count
            if cumulative >= rank:
                break
        lower, upper = histogram_bounds(index)
        result[name] = {"lower_bound_ns": lower, "upper_bound_ns": upper}
    return result


def summarize_operations(host_rows):
    totals = defaultdict(lambda: defaultdict(int))
    errors = defaultdict(lambda: defaultdict(int))
    hosts = {}
    histograms = defaultdict(lambda: [0]*65)
    host_histograms = defaultdict(lambda: defaultdict(lambda: [0]*65))
    mergeable = defaultdict(lambda: True)
    for host, rows in host_rows:
        hosts.setdefault(host, {"total_calls": 0, "cumulative_latency_ns": 0})
        for row in rows:
            op = row["operation"]
            if "histogram" not in row:
                mergeable[op] = False
            else:
                histogram_quantiles(row["histogram"])
                if sum(row["histogram"]) != row["total_calls"]:
                    raise ValueError("histogram count mismatch")
                for i, count in enumerate(row["histogram"]):
                    histograms[op][i] += count
                    host_histograms[op][host][i] += count
            for key in COUNTERS:
                totals[op][key] += row[key]
            for code, count in parse_errors(row["errors"]).items():
                errors[op][code] += count
            for key in hosts[host]:
                hosts[host][key] += row[key]
    if any(value > 2**64 - 1 or value < 0 for values in totals.values() for value in values.values()):
        raise ValueError("operation aggregate counter overflow")
    operations = []
    for op, values in totals.items():
        operations.append(dict(values, operation=op, errors=dict(errors[op]),
                               avg_latency_ns=values["cumulative_latency_ns"] / values["total_calls"] if values["total_calls"] else 0,
                               cluster_percentiles=None))
    for row in operations:
        op = row["operation"]
        if mergeable[op]:
            row["histogram"] = histograms[op]
            row["cluster_percentiles"] = histogram_quantiles(histograms[op])
            per_host = {host: histogram_quantiles(counts) for host, counts in host_histograms[op].items()}
            row["host_percentiles"] = per_host
            row["host_percentile_distribution"] = {}
            for name in QUANTILES:
                upper = sorted(q[name]["upper_bound_ns"] for q in per_host.values())
                row["host_percentile_distribution"][name] = {"host_count":len(upper), "min_upper_bound_ns":min(upper), "max_upper_bound_ns":max(upper), "median_upper_bound_ns":upper[(len(upper)-1)//2]}
            row["histogram_provenance"] = {"method":"fixed_log2_integer", "sample_provenance":"all_completions", "bounds":"inclusive", "unit":"ns", "bucket_count":65}
    operations.sort(key=lambda row: (-row["cumulative_latency_ns"], row["operation"]))
    costs = [data["cumulative_latency_ns"] for data in hosts.values()]
    mean = sum(costs) / len(costs) if costs else 0
    return {"operations": operations, "hosts": hosts,
            "host_imbalance": {"max_to_mean_cost_ratio": max(costs) / mean if mean else None,
                               "min_cost_ns": min(costs) if costs else None,
                               "max_cost_ns": max(costs) if costs else None}}
