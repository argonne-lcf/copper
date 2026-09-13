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
        self.clush_log = self.work / "clush.log"
        self.fail_launch = self.work / "fail-launch"
        self.no_mount = self.work / "no-mount"
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

    def run_launcher(self):
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
            ["bash", str(LAUNCHER), "-s", "0", "-d", str(self.work / "logs"), "-v", str(self.mount)],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=20,
        )

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


if __name__ == "__main__":
    unittest.main()
