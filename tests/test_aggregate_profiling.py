import csv
import importlib.util
import json
import os
import pathlib
import tempfile
import unittest
from unittest import mock

P=pathlib.Path(__file__).parents[1]/"scripts"/"aggregate_profiling.py"
spec=importlib.util.spec_from_file_location("aggregate_profiling", P)
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

class AggregateTests(unittest.TestCase):
    def test_classification_lives_in_aggregator(self):
        self.assertEqual(m.classify_path("metadata_ttl", "/x/python311.zip"), "python_zip_probe")
        self.assertEqual(m.classify_path("data", "/x/site-packages/torch/lib/a.so"), "torch_native_library")

    def _bundle(self, root, stem="nid-1-42-1-final", schema=2, value="3", missing=None,
                host=None, pid=None, sequence=None, kind=None):
        if host is None or pid is None or sequence is None or kind is None:
            parsed_host, parsed_pid, parsed_sequence, parsed_kind = stem.rsplit("-", 3)
            host = parsed_host if host is None else host
            pid = int(parsed_pid) if pid is None else pid
            sequence = int(parsed_sequence) if sequence is None else sequence
            kind = parsed_kind if kind is None else kind
        names=[f"{stem}-profiling_aggregate.csv",f"{stem}-profiling_operations.csv",f"{stem}-profiling_top_paths.csv"]
        contents=[f"metric,value\ntotal_counted_fuse_operations,{value}\ntotal_cache_hits,1\ntotal_cache_misses,1\ntotal_negative_results,0\ncumulative_measured_latency_ns,30\npaths_seen,3\npaths_retained,1\npaths_dropped,0\nother_events,0\nmd_enoent_ttl_stores,0\nmd_enoent_ttl_serves,0\nmd_enoent_ttl_expires,0\nmd_enoent_ttl_clears,0\n",
                  "operation,total_calls,cumulative_latency_ns,min_ns,p50_ns,p90_ns,p95_ns,p99_ns,p999_ns,max_ns,sample_count,sampling_method,cache_hits,cache_misses,negative_results,source_none,source_metadata,source_data,source_tree,source_bypass,errors\nread,3,30,10,10,30,30,30,30,30,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,\n",
                  "source,path,total_events,success,cache_hits,cache_misses,negative_results,ttl_stores,ttl_serves,ttl_expires,ttl_clears,errors\ndata,/x,3,1,1,1,0,0,0,0,0,\n"]
        for name,content in zip(names,contents):
            if name != missing: (root/name).write_text(content)
        manifest=root/f"{stem}-manifest.json"
        manifest.write_text(json.dumps({"schema_version":schema,"sequence":sequence,"kind":kind,
            "timestamp_unix_ms":1,"host":host,"pid":pid,"mode":"bounded-admitted-set",
            "config":{"top_n":10,"latency_capacity":1024,"sampling_method":"reservoir_algorithm_r"},
            "paths_seen":3,"paths_retained":1,"paths_dropped":0,"other_events":0,"files":names}))
        return manifest,names

    def test_only_manifest_committed_complete_bundles_are_discovered(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            _, good=self._bundle(root)
            (root/"orphan-profiling_aggregate.csv").write_text("metric,value\nx,999\n")
            self._bundle(root,"nid-2-43-1-final",missing="nid-2-43-1-final-profiling_operations.csv")
            bundles=m.discover_committed_bundles(root)
            self.assertEqual(len(bundles),1)
            self.assertEqual([p.name for p in bundles[0].files],good)

    def test_operation_only_csv_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            (root/"nid-1-42-1-final-profiling_operations.csv").write_text(
                "operation,total_calls\nread,3\n")
            self.assertEqual(m.discover_committed_bundles(root), [])

    def test_unsupported_schema_and_malformed_numeric_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            self._bundle(root,"good-42-1-final",schema=2)
            self._bundle(root,"old-42-1-final",schema=1)
            bundles=m.discover_committed_bundles(root)
            self.assertEqual([bundle.schema_version for bundle in bundles], [2])
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            manifest,_=self._bundle(root,value="not-a-number")
            bundle=m.discover_committed_bundles(root)[0]
            with self.assertRaisesRegex(ValueError,"numeric"):
                m.read_aggregate_csv(bundle.aggregate)

    def test_only_explicitly_supported_manifest_schema_is_accepted(self):
        for schema in (1, "2", 2.0, True):
            with self.subTest(schema=schema), tempfile.TemporaryDirectory() as d:
                root=pathlib.Path(d)
                self._bundle(root, schema=schema)
                self.assertEqual(m.discover_committed_bundles(root), [])

    def test_exact_headers_and_required_numeric_fields_are_enforced(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); self._bundle(root)
            bundle=m.discover_committed_bundles(root)[0]
            bundle.aggregate.write_text("operation,total_calls\nread,3\n")
            with self.assertRaisesRegex(ValueError, "header"):
                m.read_aggregate_csv(bundle.aggregate)

        required_operations = "operation,total_calls,cumulative_latency_ns,min_ns,p50_ns,p90_ns,p95_ns,p99_ns,p999_ns,max_ns,sample_count,sampling_method,cache_hits,cache_misses,negative_results,source_none,source_metadata,source_data,source_tree,source_bypass,errors"
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); self._bundle(root)
            bundle=m.discover_committed_bundles(root)[0]
            bundle.operations.write_text(required_operations + "\nread,3,30,1,2,3,4,5,6,7,,reservoir_algorithm_r,1,1,0,0,0,3,0,0,\n")
            with self.assertRaisesRegex(ValueError, "numeric"):
                m.read_operations_csv(bundle.operations)

        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); self._bundle(root)
            bundle=m.discover_committed_bundles(root)[0]
            bundle.top_paths.write_text("source,path,total_events\ndata,/x,nope\n")
            with self.assertRaisesRegex(ValueError, "header"):
                m.read_top_paths_csv(bundle.top_paths)

    def test_manifest_required_fields_and_types_are_enforced(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); manifest,_=self._bundle(root)
            data=json.loads(manifest.read_text()); del data["sequence"]
            manifest.write_text(json.dumps(data))
            self.assertEqual(m.discover_committed_bundles(root), [])
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); manifest,_=self._bundle(root)
            data=json.loads(manifest.read_text()); data["paths_seen"]="3"
            manifest.write_text(json.dumps(data))
            self.assertEqual(m.discover_committed_bundles(root), [])
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); manifest,_=self._bundle(root)
            data=json.loads(manifest.read_text()); data["unexpected"]="field"
            manifest.write_text(json.dumps(data))
            self.assertEqual(m.discover_committed_bundles(root), [])

    def test_manifest_supported_values_and_nonnegative_counts_are_enforced(self):
        invalid_updates = (
            {"mode": "unknown-mode"},
            {"sequence": -1},
            {"timestamp_unix_ms": -1},
            {"pid": -1},
            {"paths_seen": -1},
            {"paths_retained": -1},
            {"paths_dropped": -1},
            {"other_events": -1},
        )
        for update in invalid_updates:
            with self.subTest(update=update), tempfile.TemporaryDirectory() as d:
                root=pathlib.Path(d); manifest,_=self._bundle(root)
                data=json.loads(manifest.read_text()); data.update(update)
                manifest.write_text(json.dumps(data))
                self.assertEqual(m.discover_committed_bundles(root), [])

        for key, value in (("top_n", -1), ("latency_capacity", -1),
                           ("sampling_method", "unknown-sampler")):
            with self.subTest(config_key=key), tempfile.TemporaryDirectory() as d:
                root=pathlib.Path(d); manifest,_=self._bundle(root)
                data=json.loads(manifest.read_text()); data["config"][key]=value
                manifest.write_text(json.dumps(data))
                self.assertEqual(m.discover_committed_bundles(root), [])

    def test_malformed_manifest_is_ignored(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            (root/"bad-manifest.json").write_text("{")
            self.assertEqual(m.discover_committed_bundles(root),[])

    def test_schema_v2_ttl_fields_override_source_for_classification_and_summary(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            log=root/"log"; final=log/"profiling"/"final"; final.mkdir(parents=True)
            self._bundle(final)
            top=final/"nid-1-42-1-final-profiling_top_paths.csv"
            top.write_text(
                "source,path,total_events,success,cache_hits,cache_misses,negative_results,ttl_stores,ttl_serves,ttl_expires,ttl_clears,errors\n"
                "metadata,/x/python311.zip,2,0,0,0,0,1,1,0,0,\n"
            )
            aggregate=final/"nid-1-42-1-final-profiling_aggregate.csv"
            aggregate.write_text(aggregate.read_text().replace("paths_seen,3", "paths_seen,5").replace("paths_dropped,0", "paths_dropped,3").replace("other_events,0", "other_events,3").replace("md_enoent_ttl_stores,0", "md_enoent_ttl_stores,1").replace("md_enoent_ttl_serves,0", "md_enoent_ttl_serves,1"))
            manifest=final/"nid-1-42-1-final-manifest.json"
            data=json.loads(manifest.read_text()); data.update(paths_seen=5, paths_dropped=3, other_events=3)
            manifest.write_text(json.dumps(data))
            with mock.patch("sys.argv", [str(P), str(log), "cluster", "--legacy-unverified", "--expected-host", "nid-1"]):
                self.assertEqual(m.main(), 0)
            summary=(log/"profiling"/"cluster"/"cluster-profiling_summary.md").read_text()
            self.assertIn("| `python_zip_probe` | 1 | 2 | `/x/python311.zip` |", summary)
            self.assertIn("Repeated ENOENT probe paths are present.", summary)

    def test_schema_v2_preserves_integer_totals_above_float_precision(self):
        huge = 2**53 + 1
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            log=root/"log"; final=log/"profiling"/"final"; final.mkdir(parents=True)
            for stem in ("nid-1-42-1-final", "nid-2-43-1-final"):
                _, names = self._bundle(final, stem=stem, value=str(huge))
                (final/names[1]).write_text(
                    "operation,total_calls,cumulative_latency_ns,min_ns,p50_ns,p90_ns,p95_ns,p99_ns,p999_ns,max_ns,sample_count,sampling_method,cache_hits,cache_misses,negative_results,source_none,source_metadata,source_data,source_tree,source_bypass,errors\n"
                    f"read,{huge},{huge},1,2,3,4,5,6,7,8,reservoir_algorithm_r,{huge},0,0,0,0,{huge},0,0,\n"
                )
                (final/names[0]).write_text(
                    (final/names[0]).read_text()
                    .replace("total_cache_hits,1", f"total_cache_hits,{huge}")
                    .replace("total_cache_misses,1", "total_cache_misses,0")
                    .replace("cumulative_measured_latency_ns,30", f"cumulative_measured_latency_ns,{huge}")
                    .replace("paths_seen,3", f"paths_seen,{huge}"))
                (final/names[2]).write_text(
                    (final/names[2]).read_text().replace("data,/x,3,1,1,1,0", f"data,/x,{huge},0,{huge},0,0"))
                manifest=final/f"{stem}-manifest.json"
                data=json.loads(manifest.read_text()); data["paths_seen"]=huge
                manifest.write_text(json.dumps(data))
            with mock.patch("sys.argv", [str(P), str(log), "cluster", "--legacy-unverified", "--expected-host", "nid-1", "--expected-host", "nid-2"]):
                self.assertEqual(m.main(), 0)
            with (log/"profiling"/"cluster"/"cluster-profiling_aggregate.csv").open() as aggregate_file:
                aggregate = dict(csv.reader(aggregate_file))
            self.assertEqual(aggregate["total_counted_fuse_operations"], str(huge * 2))
            with (log/"profiling"/"cluster"/"cluster-profiling_operations.csv").open() as operations_file:
                rows = list(csv.DictReader(operations_file))
            self.assertEqual(rows[0]["total_calls"], str(huge * 2))
            self.assertEqual(rows[0]["cumulative_latency_ns"], str(huge * 2))
            self.assertNotIn(".0", aggregate["total_counted_fuse_operations"])

    def test_manifest_bundle_identity_and_global_uniqueness_are_enforced(self):
        invalid_cases = {
            "duplicate-entry": lambda root, manifest, names: manifest.write_text(json.dumps({"schema_version":2,"files":names+[names[0]]})),
            "extra-entry": lambda root, manifest, names: ((root/"extra.csv").write_text("x\n"), manifest.write_text(json.dumps({"schema_version":2,"files":names+["extra.csv"]}))),
            "cross-stem": lambda root, manifest, names: manifest.write_text(json.dumps({"schema_version":2,"files":[names[0].replace("nid-1-42-1-final", "other"),*names[1:]]})),
            "duplicate-kind": lambda root, manifest, names: manifest.write_text(json.dumps({"schema_version":2,"files":[names[0],names[0],names[2]]})),
        }
        for label, mutate in invalid_cases.items():
            with self.subTest(label=label), tempfile.TemporaryDirectory() as d:
                root=pathlib.Path(d); manifest,names=self._bundle(root)
                if label == "cross-stem":
                    (root/names[0].replace("nid-1-42-1-final", "other")).write_text((root/names[0]).read_text())
                mutate(root,manifest,names)
                self.assertEqual(m.discover_committed_bundles(root),[])

        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); _,names=self._bundle(root)
            _,second_names=self._bundle(root, "nid-2-43-1-final")
            for first, second in zip(names, second_names):
                (root/second).unlink()
                (root/second).hardlink_to(root/first)
            self.assertEqual(m.discover_committed_bundles(root),[])

    def test_manifest_filename_uses_exact_profiler_sanitized_identity(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            canonical = "___host___-42-7-unit_snapshot"
            manifest, _ = self._bundle(
                root, canonical, host="../host\n\t\"", pid=42, sequence=7,
                kind="unit/snapshot")
            self.assertEqual(
                [bundle.manifest for bundle in m.discover_committed_bundles(root)],
                [manifest])

            data = json.loads(manifest.read_text())
            data["pid"] = 43
            manifest.write_text(json.dumps(data))
            self.assertEqual(m.discover_committed_bundles(root), [])

    def test_renamed_copy_with_duplicate_logical_manifest_identity_rejects_both(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            original_manifest, original_names = self._bundle(root)
            copied_names = [name.replace("nid-1-42-1-final", "renamed")
                            for name in original_names]
            for source, copied in zip(original_names, copied_names):
                (root / copied).write_bytes((root / source).read_bytes())
            copied_data = json.loads(original_manifest.read_text())
            copied_data["files"] = copied_names
            (root / "renamed-manifest.json").write_text(json.dumps(copied_data))

            self.assertEqual(m.discover_committed_bundles(root), [])

    def test_operation_latency_metadata_must_match_manifest_and_capacity(self):
        cases = {
            "sampling method": ("reservoir_algorithm_r", "other_sampler"),
            "sample count": (",3,reservoir_algorithm_r,", ",4,reservoir_algorithm_r,"),
        }
        for label, (old, new) in cases.items():
            with self.subTest(label=label), tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d); _, names = self._bundle(root)
                operations = root / names[1]
                operations.write_text(operations.read_text().replace(old, new))
                bundle = m.discover_committed_bundles(root)[0]
                with self.assertRaisesRegex(ValueError, "latency"):
                    m.validate_bundle_consistency(bundle)

    def test_operation_latency_zero_and_ordering_rules_are_enforced(self):
        invalid_rows = (
            "idle,0,0,0,0,0,0,0,0,0,1,reservoir_algorithm_r,0,0,0,0,0,0,0,0,",
            "idle,0,1,0,0,0,0,0,0,0,0,reservoir_algorithm_r,0,0,0,0,0,0,0,0,",
            "idle,0,0,0,0,0,0,0,0,1,0,reservoir_algorithm_r,0,0,0,0,0,0,0,0,",
            "read,3,30,10,10,30,20,30,30,30,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,",
            "read,3,30,11,11,11,11,11,11,11,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,",
            "read,3,31,10,10,10,10,10,10,10,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,",
        )
        for row in invalid_rows:
            with self.subTest(row=row), tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d); _, names = self._bundle(root)
                operations = root / names[1]
                if row.startswith("idle,0,"):
                    operations.write_text(operations.read_text() + row + "\n")
                else:
                    header = operations.read_text().splitlines()[0]
                    operations.write_text(header + "\n" + row + "\n")
                bundle = m.discover_committed_bundles(root)[0]
                with self.assertRaisesRegex(ValueError, "latency"):
                    m.validate_bundle_consistency(bundle)

    def test_zero_capacity_allows_exact_bounds_with_zero_samples(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); manifest, names = self._bundle(root)
            data = json.loads(manifest.read_text())
            data["config"]["latency_capacity"] = 0
            manifest.write_text(json.dumps(data))
            operations = root / names[1]
            operations.write_text(operations.read_text().replace(
                "10,10,30,30,30,30,30,3,reservoir_algorithm_r",
                "10,0,0,0,0,0,30,0,reservoir_algorithm_r"))

            m.validate_bundle_consistency(m.discover_committed_bundles(root)[0])

    def test_manifest_path_counts_must_match_aggregate_csv(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); manifest,_=self._bundle(root)
            data=json.loads(manifest.read_text()); data["paths_seen"]=4
            manifest.write_text(json.dumps(data))
            bundle=m.discover_committed_bundles(root)[0]
            aggregate=m.read_aggregate_csv(bundle.aggregate)
            with self.assertRaisesRegex(ValueError, "manifest path counts"):
                m.validate_manifest_path_counts(bundle, aggregate)

    def test_duplicate_operation_and_top_path_rows_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); self._bundle(root)
            bundle=m.discover_committed_bundles(root)[0]
            operation_row=bundle.operations.read_text().splitlines()[1]
            with bundle.operations.open("a") as operations:
                operations.write(operation_row + "\n")
            with self.assertRaisesRegex(ValueError, "duplicate operation"):
                m.read_operations_csv(bundle.operations)

        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); self._bundle(root)
            bundle=m.discover_committed_bundles(root)[0]
            path_row=bundle.top_paths.read_text().splitlines()[1]
            with bundle.top_paths.open("a") as top_paths:
                top_paths.write(path_row + "\n")
            with self.assertRaisesRegex(ValueError, "duplicate top-path"):
                m.read_top_paths_csv(bundle.top_paths)

    def test_structurally_valid_bundles_must_be_internally_consistent(self):
        cases = {
            "aggregate operation count": (0, "total_counted_fuse_operations,3", "total_counted_fuse_operations,999"),
            "aggregate cache outcomes": (0, "total_cache_hits,1", "total_cache_hits,2"),
            "aggregate cumulative latency": (0, "cumulative_measured_latency_ns,30", "cumulative_measured_latency_ns,31"),
            "operation source total": (1, ",0,0,3,0,0,", ",0,0,2,0,0,"),
            "operation outcome total": (1, ",1,1,0,0,0,3,", ",2,2,2,0,0,3,"),
            "path outcome and TTL total": (2, "data,/x,3,1,1,1,0,0,0,0,0,", "data,/x,3,1,1,1,0,1,0,0,0,"),
            "retained path count": (0, "paths_retained,1", "paths_retained,2"),
            "seen path event total": (0, "paths_seen,3", "paths_seen,4"),
            "aggregate TTL total": (0, "md_enoent_ttl_stores,0", "md_enoent_ttl_stores,1"),
        }
        for label, (file_index, old, new) in cases.items():
            with self.subTest(label=label), tempfile.TemporaryDirectory() as d:
                root=pathlib.Path(d); _,names=self._bundle(root)
                path=root/names[file_index]
                path.write_text(path.read_text().replace(old, new))
                if label in {"retained path count", "seen path event total"}:
                    manifest=root/"nid-1-42-1-final-manifest.json"
                    data=json.loads(manifest.read_text())
                    data["paths_retained" if label == "retained path count" else "paths_seen"] = int(new.rsplit(",", 1)[1])
                    manifest.write_text(json.dumps(data))
                bundle=m.discover_committed_bundles(root)[0]
                with self.assertRaisesRegex(ValueError, "inconsistent profiling bundle"):
                    m.validate_bundle_consistency(bundle)

    def test_pathless_operation_is_valid_without_a_top_path_row(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); _,names=self._bundle(root, value="4")
            aggregate=root/names[0]
            aggregate.write_text(
                aggregate.read_text()
                .replace("cumulative_measured_latency_ns,30", "cumulative_measured_latency_ns,40"))
            operations=root/names[1]
            operations.write_text(
                operations.read_text() +
                "destroy,1,10,10,10,10,10,10,10,10,1,reservoir_algorithm_r,0,0,0,1,0,0,0,0,\n")

            aggregate_data, operation_rows, path_rows = m.validate_bundle_consistency(
                m.discover_committed_bundles(root)[0])

            self.assertEqual(aggregate_data["total_counted_fuse_operations"], 4)
            self.assertEqual([row["operation"] for row in operation_rows], ["read", "destroy"])
            self.assertEqual([(row["source"], row["path"]) for row in path_rows], [("data", "/x")])

    def test_duplicate_operation_rows_cannot_hide_aggregate_mismatch(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); _,names=self._bundle(root, value="999")
            operations=root/names[1]
            operations.write_text(
                operations.read_text().replace(
                    "read,3,30,10,10,30,30,30,30,30,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,\n",
                    "read,3,30,10,10,30,30,30,30,30,3,reservoir_algorithm_r,1,1,0,0,0,3,0,0,\n"
                    "read,4,40,10,10,40,40,40,40,40,4,reservoir_algorithm_r,0,0,0,0,0,4,0,0,\n"))
            bundle=m.discover_committed_bundles(root)[0]
            with self.assertRaisesRegex(ValueError, "duplicate operation"):
                m.validate_bundle_consistency(bundle)

    def test_main_rejects_internally_inconsistent_committed_bundle(self):
        with tempfile.TemporaryDirectory() as d:
            log=pathlib.Path(d)/"log"
            final=log/"profiling"/"final"; final.mkdir(parents=True)
            _,names=self._bundle(final, value="999")
            operations=final/names[1]
            operation_row=operations.read_text().splitlines()[1]
            with operations.open("a") as output:
                output.write(operation_row.replace("read,3", "read,4", 1) + "\n")
            with mock.patch("sys.argv", [str(P), str(log), "cluster"]):
                self.assertEqual(m.main(), 1)
            self.assertFalse((log/"profiling"/"cluster").exists())

class CoverageTests(AggregateTests):
    def test_cli_requires_expected_roster(self):
        with tempfile.TemporaryDirectory() as d:
            self._bundle(pathlib.Path(d))
            with mock.patch("sys.argv", [str(P), d]):
                self.assertEqual(m.main(), 1)

    def test_exact_roster_missing_host_requires_explicit_partial(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self._bundle(root)
            bundles = m.discover_committed_bundles(root)
            with self.assertRaisesRegex(ValueError, "missing"):
                m.select_bundles(bundles, ["nid-1", "nid-2"], legacy_unverified=True)
            selected, coverage = m.select_bundles(
                bundles, ["nid-1", "nid-2"], allow_partial=True, legacy_unverified=True)
            self.assertEqual(len(selected), 1)
            self.assertEqual(coverage["missing_hosts"], ["nid-2"])
            self.assertFalse(coverage["complete"])

    def test_restart_selects_only_latest_cumulative_snapshot(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            self._bundle(root, "nid-1-42-1-final")
            latest, _ = self._bundle(root, "nid-1-43-2-final")
            selected, coverage = m.select_bundles(m.discover_committed_bundles(root),
                ["nid-1"], legacy_unverified=True)
            self.assertEqual([b.manifest for b in selected], [latest])
            self.assertFalse(coverage["complete"])
            self.assertEqual(len(coverage["discarded_snapshots"]), 1)

    def test_recovered_snapshot_is_nonfinal_and_requires_partial(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self._bundle(root, "nid-1-42-2-periodic")
            bundles = m.discover_committed_bundles(root)
            with self.assertRaisesRegex(ValueError, "non-final"):
                m.select_bundles(bundles, ["nid-1"], legacy_unverified=True)
            _, coverage = m.select_bundles(bundles, ["nid-1"],
                legacy_unverified=True, allow_partial=True)
            self.assertFalse(coverage["final"])
            self.assertFalse(coverage["complete"])

    def test_ambiguous_restart_sequence_refuses_to_guess(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            self._bundle(root, "nid-1-42-1-final")
            self._bundle(root, "nid-1-43-1-final")
            with self.assertRaisesRegex(ValueError, "ambiguous"):
                m.select_bundles(m.discover_committed_bundles(root), ["nid-1"], legacy_unverified=True)

    def test_cli_publishes_one_generation_with_breakdown_and_coverage(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self._bundle(root)
            args = [str(P), d, "cluster", "--legacy-unverified", "--expected-host", "nid-1"]
            with mock.patch("sys.argv", args):
                self.assertEqual(m.main(), 0)
            current = root / "profiling" / "cluster" / "cluster-current"
            self.assertTrue(current.is_symlink())
            report = json.loads((current / "cluster-report.json").read_text())
            self.assertFalse(report["coverage"]["complete"])
            self.assertEqual(report["operations"][0]["source_data"], 3)
            self.assertIsNone(report["operations"][0]["cluster_percentiles"])
            (root / "nid-1-42-1-final-profiling_aggregate.csv").write_text("broken")
            pinned = current.resolve()
            with mock.patch("sys.argv", args):
                self.assertEqual(m.main(), 1)
            self.assertEqual(current.resolve(), pinned)

class Schema3Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import subprocess
        cls.build = tempfile.TemporaryDirectory()
        cls.writer = pathlib.Path(cls.build.name) / "writer"
        source = cls.writer.with_suffix(".cpp")
        source.write_text(r'''#include "metric/profiler_core.h"
#include <cstdlib>
int main(int argc, char** argv) {
 setenv("COPPER_RUN_ID", "run-1", 1);
 setenv("COPPER_DAEMON_GENERATION", "gen-1", 1);
 ProfilerConfig config{2,false,4,true};
 ProfilerCore core(config,1);
 core.complete(0,0,ProfileOutcome::success,0,"/x",ProfileSource::data);
 core.complete(0,1,ProfileOutcome::cache_hit,0,"/x",ProfileSource::data);
 core.complete(0,10,ProfileOutcome::cache_miss,5,"/y",ProfileSource::data);
 core.complete(0,100,ProfileOutcome::success,0,"/z",ProfileSource::data);
 if (argc > 2) {
  if (!write_snapshot_bundle_atomic(argv[1],"host-a",42,1,"periodic",core.snapshot_and_reset(),config).success) return 1;
  core.complete(0,4,ProfileOutcome::success,0,"/new",ProfileSource::data);
  return write_snapshot_bundle_atomic(argv[1],"host-a",42,2,"final",core.snapshot(),config).success ? 0 : 1;
 }
 return write_snapshot_bundle_atomic(argv[1],"host-a",42,1,"final",core.snapshot(),config).success ? 0 : 1;
}''')
        repo = P.parents[1]
        compiler = os.environ.get("CXX", "g++")
        subprocess.run([compiler, "-std=c++20", "-O2", "-pthread", "-I"+str(repo/"src"), str(source), str(repo/"src/metric/profiler_core.cpp"), "-o", str(cls.writer)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def emit(self, root):
        import subprocess
        subprocess.run([str(self.writer), str(root)], check=True)
        return next(root.glob("*-manifest.json"))

    def test_schema3_corruption_and_malformed_commit_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); manifest = self.emit(root)
            data = json.loads(manifest.read_text())
            target = root / data["files"][0]
            raw = target.read_bytes(); target.write_bytes(raw.replace(b"4", b"5", 1))
            with self.assertRaisesRegex(ValueError, "integrity"):
                m.discover_committed_bundles(root)
            target.write_bytes(raw)
            data["histogram"]["bucket_count"] = 64
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "schema3"):
                m.discover_committed_bundles(root)

    def clone(self, root, original, sequence, epoch=0, semantics="cumulative", generation="gen-1", run="run-1", kind="final"):
        data = json.loads(original.read_text())
        old_stem = original.name.removesuffix("-manifest.json")
        new_stem = f"host-a-42-{sequence}-{kind}"
        names = []
        integrity = {}
        for name in data["files"]:
            new_name = name.replace(old_stem, new_stem)
            (root / new_name).write_bytes((root / name).read_bytes())
            names.append(new_name); integrity[new_name] = data["file_integrity"][name]
        data.update(sequence=sequence, timestamp_unix_ms=data["timestamp_unix_ms"]+sequence,
                    reset_epoch=epoch, semantics=semantics, daemon_generation=generation,
                    run_id=run, kind=kind, files=names, file_integrity=integrity)
        target = root / (new_stem + "-manifest.json")
        target.write_text(json.dumps(data)); return target

    def test_closed_epochs_and_latest_snapshot_are_selected_without_overlap(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); original = self.emit(root)
            closed = self.clone(root, original, 2, semantics="interval", kind="periodic")
            latest = self.clone(root, original, 3, epoch=1)
            selected, coverage = m.select_bundles(m.discover_committed_bundles(root), ["host-a"])
            self.assertEqual([b.manifest for b in selected], [closed, latest])
            self.assertTrue(coverage["complete"])
            self.assertEqual(coverage["run_id"], "run-1")
            self.assertEqual(len(coverage["discarded_snapshots"]), 1)

    def test_unified_cli_emits_verified_schema3_report_and_comparison(self):
        import subprocess
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self.emit(root)
            cli = P.parent / "copper-report"
            result = subprocess.run(["python3", str(cli), str(root), "--expected-host", "host-a", "--run-id", "run-1"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            current = root / "profiling/cluster/cluster-current"
            report = json.loads((current / "cluster-report.json").read_text())
            self.assertTrue(report["coverage"]["complete"])
            self.assertEqual(report["run_id"], "run-1")
            self.assertEqual(report["operations"][0]["cluster_percentiles"]["p99"]["upper_bound_ns"], 127)
            self.assertEqual(report["receipts"]["cache"]["status"], "unavailable")
            self.assertEqual(report["compatibility"]["mode"], "bounded-heavy-hitter")
            self.assertIn("error_bound", report["paths"][0])
            compared = subprocess.run(["python3", str(P.parent / "copper-compare"), str(current), str(current)], capture_output=True, text=True)
            self.assertEqual(compared.returncode, 0, compared.stderr)
            comparison = json.loads(compared.stdout)
            self.assertTrue(comparison["compatible"])
            self.assertEqual(comparison["operations"][0]["total_calls"]["delta"], 0)

    def refresh_integrity(self, manifest):
        import hashlib
        data = json.loads(manifest.read_text())
        for name in data["files"]:
            raw = (manifest.parent / name).read_bytes()
            data["file_integrity"][name] = {"size_bytes":len(raw), "sha256":hashlib.sha256(raw).hexdigest()}
        manifest.write_text(json.dumps(data))

    def test_same_epoch_cumulative_counter_regression_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); original = self.emit(root)
            latest = self.clone(root, original, 2)
            data = json.loads(latest.read_text())
            operations = root / next(n for n in data["files"] if n.endswith("operations.csv"))
            operations.write_text(operations.read_text().replace(",111,", ",110,"))
            self.refresh_integrity(latest)
            with self.assertRaisesRegex(ValueError, "regress"):
                m.select_bundles(m.discover_committed_bundles(root), ["host-a"])

    def test_epoch_identity_and_histogram_rejection_matrix(self):
        cases = [
            ("overlap", dict(sequence=2, semantics="interval"), dict(sequence=3), "overlap"),
            ("run", dict(sequence=2, run="other"), None, "identity"),
            ("generation", dict(sequence=2, generation="other"), None, "identity"),
            ("gap", dict(sequence=2, epoch=2), None, "epochs"),
            ("reset regression", dict(sequence=2, epoch=1), dict(sequence=3), "regressed"),
        ]
        for label, second, third, message in cases:
            with self.subTest(label=label), tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d); original = self.emit(root)
                self.clone(root, original, **second)
                if third: self.clone(root, original, **third)
                with self.assertRaisesRegex(ValueError, message):
                    m.select_bundles(m.discover_committed_bundles(root), ["host-a"])
        for mutation in ("duplicate", "missing", "bound", "count"):
            with self.subTest(histogram=mutation), tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d); manifest = self.emit(root)
                data = json.loads(manifest.read_text())
                histogram = root / next(n for n in data["files"] if n.endswith("histogram.csv"))
                lines = histogram.read_text().splitlines()
                if mutation == "duplicate": lines.append(lines[1])
                elif mutation == "missing": lines.pop()
                elif mutation == "bound": lines[1] = lines[1].replace(",0,0,0,", ",0,0,1,")
                else: lines[1] = lines[1].rsplit(",",1)[0] + ",999"
                histogram.write_text("\n".join(lines)+"\n"); self.refresh_integrity(manifest)
                with self.assertRaisesRegex(ValueError, "histogram"):
                    m.validate_bundle_consistency(m.discover_committed_bundles(root)[0])

    def test_unknown_identity_is_not_verified_even_in_partial_mode(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); manifest = self.emit(root)
            data = json.loads(manifest.read_text()); data["daemon_generation"] = None
            manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "unknown"):
                m.select_bundles(m.discover_committed_bundles(root), ["host-a"], allow_partial=True)

    def test_report_publishes_cluster_histogram_and_human_bounds(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self.emit(root)
            self.assertEqual(m.main([d, "--expected-host", "host-a"]), 0)
            current = root / "profiling/cluster/cluster-current"
            histogram = current / "cluster-profiling_histogram.csv"
            self.assertTrue(histogram.is_file())
            with histogram.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 65)
            self.assertEqual(sum(int(r["count"]) for r in rows), 4)
            summary = (current / "cluster-profiling_summary.md").read_text()
            self.assertIn("Cluster Histogram Percentile Bounds", summary)
            self.assertIn("Host Percentile Distribution", summary)

    def test_same_epoch_histogram_bucket_regression_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); original = self.emit(root)
            latest = self.clone(root, original, 2)
            data = json.loads(latest.read_text())
            histogram = root / next(n for n in data["files"] if n.endswith("histogram.csv"))
            lines = histogram.read_text().splitlines()
            lines[5] = lines[5].rsplit(",",1)[0] + ",0"
            lines[6] = lines[6].rsplit(",",1)[0] + ",1"
            histogram.write_text("\n".join(lines)+"\n"); self.refresh_integrity(latest)
            with self.assertRaisesRegex(ValueError,"regressed"):
                m.select_bundles(m.discover_committed_bundles(root), ["host-a"])

    def test_real_cpp_reset_intervals_aggregate_exactly_once(self):
        import subprocess
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            subprocess.run([str(self.writer), d, "reset"], check=True)
            self.assertEqual(len(list(root.glob("*-manifest.json"))), 2)
            self.assertEqual(m.main([d, "--expected-host", "host-a"]), 0)
            report = json.loads((root / "profiling/cluster/cluster-report.json").read_text())
            self.assertEqual(report["aggregate"]["total_counted_fuse_operations"], 5)
            self.assertEqual(report["hosts"]["host-a"]["total_calls"], 5)
            self.assertEqual(sum(report["operations"][0]["histogram"]), 5)

    def test_schema3_counters_are_uint64_and_csv_shape_is_exact(self):
        for mutation in ("negative", "overflow", "extra", "error"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d); manifest = self.emit(root)
                data = json.loads(manifest.read_text())
                operations = root / next(n for n in data["files"] if n.endswith("operations.csv"))
                text = operations.read_text()
                if mutation == "negative": text = text.replace(",111,", ",-1,")
                elif mutation == "overflow": text = text.replace(",111,", f",{2**64},")
                elif mutation == "extra": text = text.rstrip("\n") + ",extra\n"
                else: text = text.replace("5:1", "5:999")
                operations.write_text(text); self.refresh_integrity(manifest)
                with self.assertRaises(ValueError):
                    m.validate_bundle_consistency(m.discover_committed_bundles(root)[0])

    def test_real_cpp_schema3_bundle_is_accepted(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d); self.emit(root)
            bundles = m.discover_committed_bundles(root)
            self.assertEqual(len(bundles), 1)
            aggregate, operations, paths = m.validate_bundle_consistency(bundles[0])
            self.assertEqual(aggregate["total_counted_fuse_operations"], 4)
            self.assertEqual(sum(operations[0]["histogram"]), 4)
            self.assertTrue(any(p["error_bound"] > 0 for p in paths))

if __name__ == "__main__": unittest.main()
