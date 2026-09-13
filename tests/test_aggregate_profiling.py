import csv
import importlib.util
import json
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
        for schema in (1, 3, "2", 2.0, True):
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
            with mock.patch("sys.argv", [str(P), str(log), "cluster"]):
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
            with mock.patch("sys.argv", [str(P), str(log), "cluster"]):
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

if __name__ == "__main__": unittest.main()
