import importlib.util
import pathlib
import sys
import tempfile
import unittest

SCRIPTS = pathlib.Path(__file__).parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

class ReportTests(unittest.TestCase):
    def test_preserves_sources_errors_and_ranks_cumulative_cost(self):
        import copper_report as report
        rows = [dict(operation="read", total_calls=2, cumulative_latency_ns=100,
                     cache_hits=0, cache_misses=1, negative_results=0,
                     source_none=0, source_metadata=0, source_data=2, source_tree=0,
                     source_bypass=0, errors="5:1", sample_count=2),
                dict(operation="stat", total_calls=1, cumulative_latency_ns=200,
                     cache_hits=0, cache_misses=0, negative_results=1,
                     source_none=0, source_metadata=1, source_data=0, source_tree=0,
                     source_bypass=0, errors="2:1", sample_count=1)]
        result = report.summarize_operations([("a", rows), ("b", [rows[0]])])
        self.assertEqual([r["operation"] for r in result["operations"]], ["read", "stat"])
        self.assertEqual(result["operations"][0]["source_data"], 4)
        self.assertEqual(result["operations"][0]["errors"], {"5": 2})
        self.assertEqual(result["hosts"]["a"]["cumulative_latency_ns"], 300)
        self.assertEqual(result["host_imbalance"]["max_to_mean_cost_ratio"], 1.5)
        self.assertIsNone(result["operations"][0]["cluster_percentiles"])

    def test_generation_publication_checksums_and_failed_switch_preserve_last_good(self):
        import copper_report as report
        import hashlib
        import json
        import os
        from unittest import mock
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            first = report.publish_generation(root, "cluster", {"cluster-report.json": b'{"first":true}', "cluster-summary.md": b"first"})
            manifest = json.loads((root / "cluster-current" / "manifest.json").read_text())
            self.assertEqual(manifest["checksums"]["cluster-summary.md"], hashlib.sha256(b"first").hexdigest())
            real_replace = os.replace
            def fail_switch(source, target):
                if pathlib.Path(target).name == "cluster-current":
                    raise OSError("switch failed")
                return real_replace(source, target)
            with mock.patch.object(report.os, "replace", side_effect=fail_switch):
                with self.assertRaisesRegex(OSError, "switch failed"):
                    report.publish_generation(root, "cluster", {"cluster-report.json": b"second", "cluster-summary.md": b"second"})
            self.assertEqual((root / "cluster-current").resolve(), first)
            self.assertEqual((root / "cluster-current" / "cluster-summary.md").read_bytes(), b"first")
            self.assertEqual((root / "cluster-summary.md").read_bytes(), b"first")

    def test_optional_receipts_are_unavailable_without_evidence_and_run_scoped(self):
        import copper_report as report
        import json
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            result = report.join_receipts("run-1", {})
            self.assertEqual(set(result), {"startup", "workload", "cleanup", "cache"})
            self.assertTrue(all(r["status"] == "unavailable" for r in result.values()))
            receipt = root / "startup.json"
            receipt.write_text(json.dumps({"run_id": "other", "elapsed_ms": 12}))
            result = report.join_receipts("run-1", {"startup": receipt})
            self.assertEqual(result["startup"]["status"], "mismatch")
            self.assertIsNone(result["startup"]["data"])
            receipt.write_text(json.dumps({"run_id": "run-1", "elapsed_ms": 12}))
            result = report.join_receipts("run-1", {"startup": receipt})
            self.assertEqual(result["startup"]["data"]["elapsed_ms"], 12)
            self.assertEqual(result["startup"]["status"], "available")

class ComparisonTests(unittest.TestCase):
    def base(self):
        return {"report_schema_version":1, "run_id":"a", "operations":[],
                "compatibility":{"schema_version":3,"mode":"aggregate-only","config":{},
                                 "histogram":{"bucket_count":65}},
                "coverage":{"complete":True,"expected_hosts":["a"],"observed_hosts":["a"]}}

    def test_comparison_lists_mismatches_without_deltas(self):
        import copper_report as report
        import copy
        a = self.base()
        for field in ("schema_version", "mode", "config", "histogram"):
            b = copy.deepcopy(a); b["compatibility"][field] = "different"
            result = report.compare_reports(a,b)
            self.assertFalse(result["compatible"])
            self.assertEqual(result["mismatches"][0]["field"],field)
            self.assertEqual(result["operations"],[])
        b = copy.deepcopy(a); b["coverage"]["complete"] = False
        self.assertFalse(report.compare_reports(a,b)["compatible"])
        b = copy.deepcopy(a); b["coverage"]["expected_hosts"] = ["b"]
        self.assertFalse(report.compare_reports(a,b)["compatible"])

    def test_comparison_cli_validates_generation_inventory_before_reading(self):
        import copper_report as report
        import json
        import subprocess
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            generation = report.publish_generation(root,"a",{"a-report.json":json.dumps(self.base()).encode()})
            (generation/"a-report.json").write_text("{}")
            result = subprocess.run(["python3",str(SCRIPTS/"copper-compare"),str(root/"a-current"),str(root/"a-current")],capture_output=True,text=True)
            self.assertEqual(result.returncode,1)
            self.assertIn("checksum",result.stderr)

    def test_operation_sum_overflow_fails_explicitly(self):
        import copper_report as report
        row = dict.fromkeys(report.COUNTERS,0)
        row.update(operation="read", errors="",total_calls=1,cumulative_latency_ns=2**64-1)
        with self.assertRaisesRegex(ValueError,"overflow"):
            report.summarize_operations([("a",[row]),("b",[row])])

    def test_histogram_count_overflow_fails_explicitly(self):
        import copper_report as report
        counts = [0]*65; counts[0] = 2**64
        with self.assertRaisesRegex(ValueError,"overflow"):
            report.histogram_quantiles(counts)

class HistogramTests(unittest.TestCase):
    def test_merge_exact_counts_cluster_bounds_and_host_distribution(self):
        import copper_report as report
        def row(counts):
            return dict(operation="read", total_calls=sum(counts), cumulative_latency_ns=10*sum(counts),
                        cache_hits=0, cache_misses=0, negative_results=0, source_none=sum(counts),
                        source_metadata=0, source_data=0, source_tree=0, source_bypass=0,
                        sample_count=0, errors="", histogram=counts)
        a = [0]*65; a[4] = 100
        b = [0]*65; b[7] = 1
        result = report.summarize_operations([("a", [row(a)]), ("b", [row(b)])])
        op = result["operations"][0]
        self.assertEqual(op["cluster_percentiles"]["p99"], {"lower_bound_ns":8, "upper_bound_ns":15})
        self.assertEqual(op["cluster_percentiles"]["p999"]["upper_bound_ns"], 127)
        self.assertEqual(op["host_percentile_distribution"]["p99"]["max_upper_bound_ns"], 127)
        self.assertEqual(op["histogram"][4], 100)
        self.assertEqual(report.histogram_bounds(64), (2**63,2**64-1))

if __name__ == "__main__": unittest.main()
