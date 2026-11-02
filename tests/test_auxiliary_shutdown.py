import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / 'scripts/auxiliary_shutdown.py'

class ShutdownTests(unittest.TestCase):
    def test_missing_identity_fails_closed_in_real_entrypoints(self):
        for site in ('aurora', 'frontier'):
            p = subprocess.run(['bash', str(ROOT / f'scripts/stop_copper_{site}.sh'), '--manifest', '/definitely/missing', '--job-id', 'test'], capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0)
            self.assertIn('manifest', p.stderr.lower())
            self.assertNotIn('Stopping Copper Gracefully', p.stdout)

    def test_legacy_options_resolve_manifest_without_discovery(self):
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            env = dict(os.environ, PBS_JOBID='123.server')
            p = subprocess.run(['bash', str(ROOT / 'scripts/stop_copper_aurora.sh'),
                                '-d', td, '-v', '/tmp/expected'], env=env,
                               capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0)
            self.assertIn(str(Path(td) / '123' / 'daemon-identity.json'), p.stderr)
            self.assertNotIn('unrecognized arguments', p.stderr)

    def test_exact_mount_parser_does_not_confuse_parent_or_escaped_mount(self):
        self.assertTrue(HELPER.exists())
        spec = importlib.util.spec_from_file_location('shutdown', HELPER)
        mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
        self.assertTrue(hasattr(mod, 'exact_mount'), 'missing exact mount parser')
        data = '1 0 0:1 / / rw - ext4 /dev/root rw\n2 1 0:2 / /tmp/a\\040b rw - fuse.copper copper rw\n'
        self.assertIsNone(mod.exact_mount('/tmp/a', data))
        self.assertEqual(mod.exact_mount('/tmp/a b', data)['mount_id'], 2)
        self.assertIsNone(mod.exact_mount('/tmp/a b/child', data))

    def test_local_shutdown_targets_only_manifest_identity(self):
        import tempfile, socket, pwd
        with tempfile.TemporaryDirectory() as td:
            child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)', '-vpath', td])
            other = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])
            try:
                start = int(Path(f'/proc/{child.pid}/stat').read_text().rsplit(')', 1)[1].split()[19])
                record = {'pid': child.pid, 'starttime': start, 'uid': os.getuid(), 'user': pwd.getpwuid(os.getuid()).pw_name, 'mount': td, 'mount_identity': None, 'exe': os.readlink(f'/proc/{child.pid}/exe'), 'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip()}
                host = socket.gethostname()
                manifest = {'schema_version': 1, 'job_id': 'test', 'expected_hosts': [host], 'nodes': {host: record}}
                path = Path(td)/'identity.json'; path.write_text(json.dumps(manifest)); path.chmod(0o600)
                command = [sys.executable, str(HELPER), '--manifest', str(path), '--job-id', 'test', '--local', '--timeout', '0.2']
                for bad in (dict(record, starttime=start+1), dict(record, uid=os.getuid()+1),
                            dict(record, user='not-the-caller'), dict(record, mount='/different/mount'),
                            dict(record, boot_id='different-boot'), dict(record, exe='/different/exe')):
                    manifest['nodes'][host] = bad; path.write_text(json.dumps(manifest))
                    p = subprocess.run(command, capture_output=True, text=True)
                    self.assertNotEqual(p.returncode, 0); self.assertIsNone(child.poll())
                # Non-root process user and every identity field must agree before any signal.
                manifest['nodes'][host] = record; path.write_text(json.dumps(manifest))
                p = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(p.returncode, 0, p.stderr+p.stdout)
                receipt = json.loads(p.stdout); self.assertTrue(receipt['ok']); self.assertFalse(receipt['mount_active']); self.assertTrue(receipt['process_exited'])
                child.wait(timeout=3); self.assertIsNone(other.poll())
            finally:
                for process in (child, other):
                    if process.poll() is None: process.kill()
                    process.wait()

    def test_distributed_transport_failure_has_one_failed_receipt_per_host(self):
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            manifest = Path(td)/'manifest.json'
            manifest.write_text(json.dumps({'schema_version': 1, 'job_id': 'j', 'expected_hosts': ['host-a', 'host-b'], 'nodes': {'host-a': {}, 'host-b': {}}})); manifest.chmod(0o600)
            env = dict(os.environ, PATH='/nonexistent')
            for scheduler in ('aurora', 'frontier'):
                p = subprocess.run([sys.executable, str(HELPER), '--scheduler', scheduler, '--manifest', str(manifest), '--job-id', 'j'], env=env, capture_output=True, text=True)
                self.assertNotEqual(p.returncode, 0)
                self.assertTrue(p.stdout, 'must print aggregate receipts')
                result = json.loads(p.stdout)
                self.assertEqual(set(result['nodes']), {'host-a', 'host-b'})
                self.assertTrue(all(not r['ok'] for r in result['nodes'].values()))

if __name__ == '__main__': unittest.main()
