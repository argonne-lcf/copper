import json
import os
import pathlib
import subprocess
import tempfile
import textwrap
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / "scripts" / "launch_copper_aurora.sh"


class AuroraLauncherTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.work = pathlib.Path(self.tempdir.name)
        self.copper_root = self.work / "copper root"
        self.build = self.copper_root / "build"
        self.fakebin = self.work / "fake bin"
        self.build.mkdir(parents=True)
        self.fakebin.mkdir()
        self.mount = self.work / "mount path"
        self.mount_marker = self.work / "mount-active"
        self.runtime_capture = self.work / "runtime-env"
        self.launch_logs = self.work / "logs" / "123" / "logs"
        self.clush_log = self.work / "clush.log"
        self.fail_launch = self.work / "fail-launch"
        self.no_mount = self.work / "no-mount"
        self.malformed_probe = self.work / "malformed-probe"
        self.recovery_ran = self.work / "recovery-ran"
        self.recovery_hosts = self.work / "recovery-hosts"
        self.nodefile = self.work / "nodes"
        self.nodefile.write_text("node-a\nnode-b\n")
        (self.build / "alcf_aurora_copper_addressbook.txt").write_text(
            "node-a cxi://node-a\nnode-b cxi://node-b\n"
        )
        self._write_executable(
            self.fakebin / "clush",
            f"""#!/bin/bash
set -u
printf '%s\\n' "$*" >> {self._q(self.clush_log)}
remote=${{@: -1}}
if [[ "$remote" == *COPPER_%s* ]]; then
  if [[ -e {self._q(self.malformed_probe)} ]]; then
    printf 'node-a: COPPER_BROKEN mount=ok\\n'
    exit 0
  fi
  rc=0
  while read -r host; do
    if /bin/bash -c "$remote" >/tmp/copper-probe.$$ 2>&1; then
      printf '%s: %s\\n' "$host" "$(cat /tmp/copper-probe.$$)"
    else
      rc=1; printf '%s: %s\\n' "$host" "$(cat /tmp/copper-probe.$$)"
    fi
  done < {self._q(self.nodefile)}
  rm -f /tmp/copper-probe.$$
  exit "$rc"
fi
if [[ -e {self._q(self.fail_launch)} && "$remote" == *cu_fuse* ]]; then
  exit 42
fi
exec env -i HOME="$HOME" USER="$USER" /bin/bash -c "$remote"
""",
        )
        self._write_executable(
            self.fakebin / "numactl",
            """#!/bin/bash
set -u
shift
exec "$@"
""",
        )
        self._write_executable(
            self.fakebin / "fusermount3",
            f"""#!/bin/bash
rm -f {self._q(self.mount_marker)}
""",
        )
        self._write_executable(
            self.fakebin / "mountpoint",
            f"""#!/bin/bash
[[ "$1" == "-q" ]] && shift
[[ "$1" == {self._q(self.mount)} && -e {self._q(self.mount_marker)} ]]
""",
        )
        self._write_executable(
            self.build / "cu_fuse_probe",
            """#!/bin/bash
exit 0
""",
        )
        self._write_executable(
            self.build / "cu_fuse",
            f"""#!/bin/bash
printf 'PATH=%s\\nLD_LIBRARY_PATH=%s\\nFI_PROVIDER=%s\\nMARGO_CONFIG=%s\\nCOPPER_ROOT=%s\\n' \\
  "$PATH" "${{LD_LIBRARY_PATH-}}" "${{FI_PROVIDER-}}" "${{MARGO_CONFIG-}}" "${{COPPER_ROOT-}}" \\
  > {self._q(self.runtime_capture)}
[[ "${{LD_LIBRARY_PATH-}}" == {self._q(str(self.work / 'runtime libs'))} ]] || exit 86
[[ ":$PATH:" == *{self._q(':' + str(self.fakebin) + ':')}* ]] || exit 87
if [[ ! -e {self._q(self.no_mount)} ]]; then
  : > {self._q(self.mount_marker)}
fi
mkdir -p {self._q(self.launch_logs)}
printf 'provider registration completed after 100 us\n' > {self._q(self.launch_logs / 'node-a-output.log')}
printf 'provider registration completed after 300 us\n' > {self._q(self.launch_logs / 'node-b-output.log')}
python3 - <<'RECORDS'
import json, os, pathlib
for host in ('node-a', 'node-b'):
    record = dict(schema_version=1, host=host, job_id=os.environ['COPPER_JOB_ID'],
                  run_id=os.environ['COPPER_RUN_ID'], daemon_generation=os.environ['COPPER_DAEMON_GENERATION'],
                  pid=os.getpid(), uid=os.getuid(), user='tester', starttime=123,
                  boot_id='test-boot', exe='/test/cu_fuse', mount={str(self.mount)!r},
                  mount_identity=dict(mount_id=42, fstype='fuse.cu_fuse', source='cu_fuse'))
    p = pathlib.Path(os.environ['COPPER_DAEMON_IDENTITY_DIR'])/(host+'.json')
    p.write_text(json.dumps(record)); p.chmod(0o600)
RECORDS
""",
        )

    def tearDown(self):
        self.tempdir.cleanup()

    @staticmethod
    def _q(path):
        import shlex

        return shlex.quote(str(path))

    @staticmethod
    def _write_executable(path, content):
        path.write_text(textwrap.dedent(content))
        path.chmod(0o755)

    def run_launcher(self, *extra):
        env = os.environ.copy()
        env.update(
            {
                "COPPER_ROOT": str(self.copper_root),
                "PBS_JOBID": "123.server",
                "PBS_NODEFILE": str(self.nodefile),
                "USER": "tester",
                "PATH": f"{self.fakebin}:{env['PATH']}",
                "LD_LIBRARY_PATH": str(self.work / "runtime libs"),
                "FI_PROVIDER": "cxi;safe value",
                "MARGO_CONFIG": "value with spaces;$(false)",
            }
        )
        return subprocess.run(
            ["bash", str(LAUNCHER), "-s", "0", "-d", str(self.work / "logs"), "-v", str(self.mount),
             "-R", "1", "-B", "0", *extra],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=20,
        )

    def test_ready_launch_publishes_private_exact_job_manifest(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stdout)
        manifest = self.work / 'logs' / '123' / 'daemon-identity.json'
        self.assertTrue(manifest.exists(), 'ready launch did not publish daemon identities')
        data = json.loads(manifest.read_text())
        self.assertEqual(data['job_id'], '123.server')
        self.assertEqual(data['expected_hosts'], ['node-a', 'node-b'])
        self.assertEqual(set(data['nodes']), {'node-a', 'node-b'})
        self.assertEqual(manifest.stat().st_mode & 0o777, 0o600)
        summary = json.loads((manifest.parent / 'startup-summary.json').read_text())
        self.assertEqual(summary['run_id'], data['run_id'])
        self.assertEqual(summary['daemon_generation'], data['daemon_generation'])

    def test_launch_directory_is_private(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual((self.work/'logs'/'123').stat().st_mode & 0o777, 0o700)

    def test_missing_daemon_record_fails_closed_after_ready_probe(self):
        daemon = self.build / 'cu_fuse'
        daemon.write_text(daemon.read_text().replace("for host in ('node-a', 'node-b'):", "for host in ('node-a',):"))
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('daemon identity collection failed', result.stdout)
        self.assertFalse((self.work/'logs'/'123'/'daemon-identity.json').exists())

    def test_runtime_environment_is_serialized_for_remote_rank(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stdout)
        captured = self.runtime_capture.read_text()
        self.assertIn(f"PATH={self.fakebin}:", captured)
        self.assertIn(f"LD_LIBRARY_PATH={self.work / 'runtime libs'}", captured)
        self.assertIn("FI_PROVIDER=cxi;safe value", captured)
        self.assertIn("MARGO_CONFIG=value with spaces;$(false)", captured)
        self.assertIn(f"COPPER_ROOT={self.copper_root}", captured)

    def test_any_rank_startup_failure_fails_launch(self):
        self.fail_launch.touch()
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("rank startup failed", result.stdout)

    def test_missing_mount_after_successful_rank_command_fails_launch(self):
        self.no_mount.touch()
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("mount verification failed", result.stdout)

    def test_auto_trees_and_atomic_startup_summary(self):
        result = self.run_launcher("-T", "auto", "-p", str(self.mount))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("preflight: nodes=2 trees=1 segment_size=2 depth=1", result.stdout)
        summary = json.loads((self.work / "logs" / "123" / "startup-summary.json").read_text())
        self.assertEqual(summary["status"], "ready")
        self.assertEqual(summary["trees"], 1)
        self.assertEqual(summary["expected_nodes"], 2)
        self.assertEqual(summary["probe_path"], str(self.mount))
        self.assertEqual(summary["address_book"]["mode"], "facility")
        self.assertEqual(len(summary["address_book"]["sha256"]), 64)
        self.assertEqual(summary["registration_quantiles_us"]["p50"], 100)
        self.assertEqual(summary["registration_quantiles_us"]["max"], 300)
        self.assertEqual(summary["phase_failure_hosts"]["readiness"], [])
        self.assertEqual(summary["counts"]["expected"], 2)
        self.assertEqual(summary["counts"]["launched"], 2)
        self.assertEqual(summary["counts"]["mounted"], 2)
        self.assertEqual(summary["counts"]["registered"], 2)
        self.assertEqual(summary["counts"]["rpc_ready"], 2)
        self.assertEqual(summary["provenance"]["binary"], str(self.build / "cu_fuse"))
        self.assertEqual(len(summary["provenance"]["binary_sha256"]), 64)

    def test_malformed_address_book_fails_before_rank_launch(self):
        (self.build / "alcf_aurora_copper_addressbook.txt").write_text("node-a cxi://node-a\ninvalid-line\n")
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("address book validation failed", result.stdout)
        self.assertNotIn("cu_fuse", self.clush_log.read_text())

    def test_malformed_readiness_output_fails_closed(self):
        self.malformed_probe.touch()
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("readiness barrier failed", result.stdout)

    def test_bounded_recovery_hook_receives_only_failed_hosts_and_recovers(self):
        self.no_mount.touch()
        hook = self.work / "recover"
        self._write_executable(
            hook,
            f"""#!/bin/bash
set -eu
cp "$1" {self._q(self.recovery_hosts)}
test "$2" = 1
rm -f {self._q(self.no_mount)}
touch {self._q(self.mount_marker)} {self._q(self.recovery_ran)}
""",
        )
        result = self.run_launcher("-H", str(hook), "-K", "1")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.recovery_ran.exists())
        self.assertEqual(self.recovery_hosts.read_text().splitlines(), ["node-a", "node-b"])
        summary = json.loads((self.work / "logs" / "123" / "startup-summary.json").read_text())
        self.assertEqual(summary["recovery_attempts"], 1)
        self.assertEqual(summary["status"], "ready")

    def test_recovery_is_disabled_by_default(self):
        self.no_mount.touch()
        hook = self.work / "recover"
        self._write_executable(hook, f"#!/bin/bash\ntouch {self._q(self.recovery_ran)}\n")
        result = self.run_launcher("-H", str(hook))
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("recovery disabled", result.stdout)
        self.assertFalse(self.recovery_ran.exists())

    def test_category_and_timeout_options_reach_cu_fuse(self):
        result = self.run_launcher("-L", "5", "-O", "1", "-X", "9000", "-Y", "8000")
        self.assertEqual(result.returncode, 0, result.stdout)
        clush = self.clush_log.read_text()
        self.assertIn("-startup_log_level 5", clush)
        self.assertIn("-operation_log_level 1", clush)
        self.assertIn("-engine_startup_timeout_ms 9000", clush)
        self.assertIn("-parent_ready_timeout_ms 8000", clush)


if __name__ == "__main__":
    unittest.main()
