#!/usr/bin/env python3
"""Real-process acceptance tests for the profiler microbenchmark."""
import csv
import io
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIELDS = [
    "mode", "threads", "iterations_per_thread", "total_calls", "elapsed_ns",
    "ns_per_call", "paths_retained", "snapshot_count", "snapshot_elapsed_ns",
    "ns_per_snapshot", "process_peak_rss_kib",
]


class ProfilerBenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix="copper-benchmark-test-")
        cls.binary = os.environ.get("COPPER_PROFILER_BENCHMARK", "")
        if not cls.binary:
            cls.binary = str(Path(cls.work.name) / "benchmark-profiler")
            subprocess.run([
                os.environ.get("CXX", "g++"), "-std=c++20", "-O2", "-pthread", "-Isrc",
                "tests/benchmark_profiler.cpp", "src/metric/profiler_core.cpp",
                "-o", cls.binary,
            ], cwd=ROOT, check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def test_runner_rejects_invalid_options_before_creating_output(self):
        output = Path(self.work.name) / "invalid.csv"
        cases = [("--timeout", value) for value in ("nan", "inf", "-inf", "0", "-1")]
        cases += [(option, value) for option in ("--iterations", "--snapshots")
                  for value in ("0", "-1", "nan", "inf", "1.5", "1000000001")]
        for option, value in cases:
            result = subprocess.run([
                "python3", str(Path(__file__).resolve()), "--run-matrix",
                "--binary", self.binary, "--output", str(output), f"{option}={value}",
            ], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2, (option, value, result.stderr))
            self.assertFalse(output.exists(), (option, value))

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux ru_maxrss launch regression")
    def test_runner_does_not_inherit_python_parent_rss(self):
        import resource
        ballast = bytearray(64 * 1024 * 1024)
        parent_peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        output = Path(self.work.name) / "rss-isolation.csv"
        run_matrix(["--binary", self.binary, "--output", str(output),
                    "--iterations", "257", "--snapshots", "2"])
        with output.open(newline="") as stream:
            peaks = [int(row["process_peak_rss_kib"]) for row in csv.DictReader(stream)]
        self.assertEqual(len(peaks), 20)
        self.assertLess(max(peaks), parent_peak / 2,
                        "case RSS inherited the large Python runner before exec")
        self.assertEqual(len(ballast), 64 * 1024 * 1024)

    def test_matrix_runner_saves_all_cases(self):
        output = Path(self.work.name) / "matrix.csv"
        result = subprocess.run([
            "python3", str(Path(__file__).resolve()), "--run-matrix",
            "--binary", self.binary, "--output", str(output),
            "--iterations", "257", "--snapshots", "2", "--timeout", "10",
        ], capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        with output.open(newline="") as stream:
            reader = csv.DictReader(stream)
            self.assertEqual(reader.fieldnames, FIELDS)
            rows = list(reader)
        self.assertEqual(len(rows), 20)
        self.assertEqual({(r["mode"], int(r["threads"])) for r in rows},
                         {(m, t) for m in ("disabled", "aggregate", "admitted", "bounded", "full")
                          for t in (1, 2, 4, 8)})
        for row in rows:
            self.assertEqual(int(row["iterations_per_thread"]), 257)
            self.assertEqual(int(row["snapshot_count"]), 2)

    def test_invalid_case_options_fail_without_csv(self):
        bad_cases = [["--snapshots", "0"], ["--mode", "unknown"],
                     ["--mode"], ["--unknown", "1"], ["100"],
                     ["--threads", "3"], ["--threads", "16"],
                     ["--mode", "full", "--mode", "aggregate"]]
        for option, maximum in (("--threads", 8), ("--iterations", 1000000000),
                                ("--snapshots", 1000000)):
            for value in ("0", "-1", "+1", "1.5", "1x", " 1", "1 ", "",
                          "nan", "inf", "-inf", "1e3", str(maximum + 1),
                          "18446744073709551616"):
                bad_cases.append([option, value])
        # Stop at the first failure: invalid large counts must never execute work.
        for args in bad_cases:
            result = subprocess.run([self.binary, *args], capture_output=True,
                                    text=True, timeout=3)
            self.assertEqual(result.returncode, 2, args)
            self.assertEqual(result.stdout, "", args)
            self.assertIn("error:", result.stderr, args)

    def test_independent_case_matrix(self):
        iterations = 257
        unique_paths = len({i % 16 if i % 5 else 16 + i % 1008
                            for i in range(iterations)})
        for mode in ("disabled", "aggregate", "admitted", "bounded", "full"):
            for threads in (1, 2, 4, 8):
                with self.subTest(mode=mode, threads=threads):
                    result = subprocess.run([
                        self.binary, "--mode", mode, "--threads", str(threads),
                        "--iterations", str(iterations), "--snapshots", "3",
                    ], capture_output=True, text=True, check=True, timeout=30)
                    reader = csv.DictReader(io.StringIO(result.stdout))
                    self.assertEqual(reader.fieldnames, FIELDS)
                    rows = list(reader)
                    self.assertEqual(len(rows), 1)
                    row = rows[0]
                    self.assertEqual(row["mode"], mode)
                    self.assertEqual(int(row["threads"]), threads)
                    self.assertEqual(int(row["iterations_per_thread"]), iterations)
                    self.assertEqual(int(row["snapshot_count"]), 3)
                    self.assertEqual(int(row["total_calls"]),
                                     0 if mode == "disabled" else threads * iterations)
                    retained = (0 if mode in ("disabled", "aggregate") else
                                unique_paths if mode == "full" else min(64, unique_paths))
                    self.assertEqual(int(row["paths_retained"]), retained)
                    self.assertAlmostEqual(float(row["ns_per_call"]),
                                           int(row["elapsed_ns"]) / (threads * iterations),
                                           delta=0.001)
                    self.assertGreater(int(row["process_peak_rss_kib"]), 0)

    def test_default_case_output_schema(self):
        result = subprocess.run([self.binary], capture_output=True, text=True,
                                timeout=60, check=True)
        reader = csv.DictReader(io.StringIO(result.stdout))
        self.assertEqual(reader.fieldnames, FIELDS)
        rows = list(reader)
        self.assertEqual(len(rows), 1, "one invocation must measure just one case")
        row = rows[0]
        self.assertEqual(row["mode"], "aggregate")
        self.assertEqual(int(row["threads"]), 1)
        self.assertEqual(int(row["total_calls"]), int(row["iterations_per_thread"]))
        self.assertEqual(int(row["paths_retained"]), 0)
        for field in FIELDS[1:]:
            self.assertTrue(math.isfinite(float(row[field])), field)
        for field in ("elapsed_ns", "ns_per_call", "snapshot_count",
                      "snapshot_elapsed_ns", "ns_per_snapshot", "process_peak_rss_kib"):
            self.assertGreater(float(row[field]), 0, field)
        self.assertAlmostEqual(float(row["ns_per_snapshot"]),
                               int(row["snapshot_elapsed_ns"]) / int(row["snapshot_count"]),
                               delta=0.001)


def run_matrix(argv):
    """Execute each case via a fresh exec; append verified raw rows immediately."""
    import argparse

    parser = argparse.ArgumentParser(description=run_matrix.__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=100000)
    parser.add_argument("--snapshots", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("timeout must be finite and positive")
    if not 1 <= args.iterations <= 1000000000:
        parser.error("iterations must be in 1..1000000000")
    if not 1 <= args.snapshots <= 1000000:
        parser.error("snapshots must be in 1..1000000")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    modes = ("disabled", "aggregate", "admitted", "bounded", "full")
    expected = {(mode, threads) for mode in modes for threads in (1, 2, 4, 8)}
    observed = set()
    with args.output.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for mode in modes:
            for threads in (1, 2, 4, 8):
                # A direct Python fork/exec can retain Python's peak RSS on Linux.
                # The small shell must FORK the case (not exec-optimize it); the
                # trailing status command keeps the shell alive until it exits.
                result = subprocess.run([
                    "/bin/sh", "-c", '"$@"; status=$?; exit "$status"', "benchmark-launch",
                    str(Path(args.binary).resolve()), "--mode", mode,
                    "--threads", str(threads), "--iterations", str(args.iterations),
                    "--snapshots", str(args.snapshots),
                ], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                   universal_newlines=True, check=True, timeout=args.timeout)
                reader = csv.DictReader(io.StringIO(result.stdout))
                rows = list(reader)
                if reader.fieldnames != FIELDS or len(rows) != 1:
                    raise ValueError("benchmark must produce the exact schema and one case")
                row = rows[0]
                if (row["mode"] != mode or int(row["threads"]) != threads or
                        int(row["iterations_per_thread"]) != args.iterations or
                        int(row["snapshot_count"]) != args.snapshots or
                        int(row["total_calls"]) != (0 if mode == "disabled" else threads * args.iterations)):
                    raise ValueError("benchmark case/count mismatch")
                for field in FIELDS[1:]:
                    value = float(row[field])
                    if not math.isfinite(value) or value < 0:
                        raise ValueError(f"invalid measurement: {field}")
                for field in ("elapsed_ns", "ns_per_call", "snapshot_elapsed_ns",
                              "ns_per_snapshot", "process_peak_rss_kib"):
                    if float(row[field]) <= 0:
                        raise ValueError(f"nonpositive measurement: {field}")
                observed.add((mode, threads))
                writer.writerow(row)
                stream.flush()
    if observed != expected:
        raise ValueError("incomplete matrix")
    print(f"Verified {len(observed)} independent cases: {args.output}")


if __name__ == "__main__":
    import sys
    if "--run-matrix" in sys.argv:
        arguments = sys.argv[1:]
        arguments.remove("--run-matrix")
        run_matrix(arguments)
    else:
        unittest.main()
