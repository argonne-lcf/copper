import json
import os
import pathlib
import subprocess
import tempfile
import textwrap
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "copper-scale-check"


class CopperScaleCheckTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.work = pathlib.Path(self.tmp.name)
        self.fakebin = self.work / "bin"
        self.fakebin.mkdir()
        self.hostfile = self.work / "nodes"
        self.hostfile.write_text("node-a\nnode-b\n")
        self.state = self.work / "state"
        self.manifest = self.work / "manifest.json"
        self._write_executable(
            self.fakebin / "clush",
            f"""#!/bin/bash
count=0
[[ -e {self.state!s} ]] && count=$(cat {self.state!s})
count=$((count + 1)); printf '%s' "$count" > {self.state!s}
if (( count == 1 )); then
  printf 'node-a: COPPER_READY mount=ok rpc=ok path=ok profile=ok\\n'
  printf 'node-b: COPPER_NOT_READY mount=missing rpc=missing path=skip profile=skip\\n'
  exit 1
fi
printf 'node-a: COPPER_READY mount=ok rpc=ok path=ok profile=ok\\n'
printf 'node-b: COPPER_READY mount=ok rpc=ok path=ok profile=ok\\n'
""",
        )

    def tearDown(self):
        self.tmp.cleanup()

    @staticmethod
    def _write_executable(path, content):
        path.write_text(textwrap.dedent(content))
        path.chmod(0o755)

    def run_tool(self, *extra):
        env = os.environ.copy()
        env["PATH"] = f"{self.fakebin}:{env['PATH']}"
        self.rpc_probe = self.fakebin / "cu_fuse_probe"
        self._write_executable(self.rpc_probe, "#!/bin/bash\nexit 0\n")
        return subprocess.run(
            [str(TOOL), "--hostfile", str(self.hostfile), "--mount", "/mnt/copper",
             "--rpc-probe-helper", str(self.rpc_probe), "--network-type", "cxi://cxi7:1",
             "--provider-id", "0", "--address-book", "/logs/address-book.txt",
             "--timeout", "1", "--backoff", "0", "--manifest", str(self.manifest), *extra],
            env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10,
        )

    def test_delayed_rpc_readiness_retries_and_writes_exact_manifest(self):
        result = self.run_tool("--probe-path", "/mnt/copper/pkg/module.py")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("ready=1/2 missing=node-b", result.stdout)
        self.assertIn("ready=2/2 missing=-", result.stdout)
        data = json.loads(self.manifest.read_text())
        self.assertEqual(data["status"], "ready")
        self.assertEqual(data["expected_hosts"], ["node-a", "node-b"])
        self.assertEqual(sorted(data["nodes"]), ["node-a", "node-b"])
        self.assertTrue(all(v["ready"] for v in data["nodes"].values()))
        self.assertEqual(data["probe_path"], "/mnt/copper/pkg/module.py")

    def test_never_ready_times_out_and_records_failed_node(self):
        self._write_executable(self.fakebin / "clush", """#!/bin/bash
printf 'node-a: COPPER_READY mount=ok rpc=ok path=ok profile=ok\n'
printf 'node-b: COPPER_NOT_READY mount=missing rpc=missing path=skip profile=skip\n'
exit 1
""")
        result = self.run_tool()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        data = json.loads(self.manifest.read_text())
        self.assertEqual(data["status"], "failed")
        self.assertFalse(data["nodes"]["node-b"]["ready"])

    def test_manifest_reports_phase_counts(self):
        result = self.run_tool()
        self.assertEqual(result.returncode, 0, result.stdout)
        data = json.loads(self.manifest.read_text())
        self.assertEqual(data["counts"], {
            "expected": 2,
            "mounted": 2,
            "path_ready": 2,
            "profile_ready": 2,
            "registered": 0,
            "rpc_ready": 2,
        })

    def test_partial_profiles_fail_closed(self):
        self._write_executable(self.fakebin / "clush", """#!/bin/bash
printf 'node-a: COPPER_READY mount=ok rpc=ok path=ok profile=ok\n'
printf 'node-b: COPPER_NOT_READY mount=ok rpc=ok path=ok profile=missing\n'
exit 1
""")
        result = self.run_tool("--profile-glob", "/logs/*-profile.csv")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(json.loads(self.manifest.read_text())["nodes"]["node-b"]["profile"], "missing")

    def test_dead_daemon_fails_even_when_mount_exists(self):
        self._write_executable(self.fakebin / "clush", """#!/bin/bash
printf 'node-a: COPPER_READY mount=ok rpc=ok path=ok profile=ok\n'
printf 'node-b: COPPER_NOT_READY mount=ok rpc=missing path=ok profile=ok\n'
exit 1
""")
        result = self.run_tool()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        data = json.loads(self.manifest.read_text())
        self.assertEqual(data["counts"]["mounted"], 2)
        self.assertEqual(data["counts"]["rpc_ready"], 1)
        self.assertEqual(data["nodes"]["node-b"]["rpc"], "missing")

    def test_stale_registration_log_is_telemetry_not_readiness_evidence(self):
        logs = self.work / "logs"
        logs.mkdir()
        stale = logs / "node-a-output.log"
        stale.write_text("provider registration completed after 100 us\n")
        os.utime(stale, (1, 1))
        result = self.run_tool("--registration-dir", str(logs), "--registration-since-epoch", "2")
        self.assertEqual(result.returncode, 0, result.stdout)
        data = json.loads(self.manifest.read_text())
        self.assertEqual(data["counts"]["registered"], 0)
        self.assertIsNone(data["nodes"]["node-a"]["registration_us"])


if __name__ == "__main__":
    unittest.main()
