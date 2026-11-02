import json, os, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
IOCTL = ROOT/'scripts/get_copper_stats_ioctl/ioctl.py'

class IoctlTests(unittest.TestCase):
    def test_named_commands_emit_json_failures_without_creating_files(self):
        with tempfile.TemporaryDirectory() as td:
            device = Path(td)/'device'
            p = subprocess.run([sys.executable, str(IOCTL), 'data-cache-size', '--device', str(device)], capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0)
            self.assertTrue(p.stdout, 'JSON error receipt required')
            self.assertFalse(json.loads(p.stdout)['ok']); self.assertFalse(device.exists())
            device.touch()
            p = subprocess.run([sys.executable, str(IOCTL), 'data-cache-size', '--device', str(device)], capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0)
            result = json.loads(p.stdout)
            self.assertEqual(result['request'], 2300)
            self.assertEqual(result['errno'], 25)

    def test_mutations_require_explicit_consent_and_invalid_commands_are_json(self):
        for command in ('reset-fs', 'clear-cache', '1900'):
            p = subprocess.run([sys.executable, str(IOCTL), command, '--device', '/missing'], capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0); self.assertTrue(p.stdout, 'JSON error required')
            result = json.loads(p.stdout); self.assertFalse(result['ok'])
            if command != '1900': self.assertIn('--allow-mutation', result['error'])
        p = subprocess.run([sys.executable, str(IOCTL), 'reset-fs', '--allow-mutation', '--device', '/missing'], capture_output=True, text=True)
        self.assertEqual(json.loads(p.stdout)['errno'], 2)

    def test_wrapper_timeout_bounds_blocked_open_and_reset_never_deletes(self):
        with tempfile.TemporaryDirectory() as td:
            fifo = Path(td)/'fifo'; os.mkfifo(fifo)
            env = dict(os.environ, COPPER_IOCTL_TIMEOUT='0.1')
            script = IOCTL.parent/'run_ioctl.sh'
            p = subprocess.run(['bash', str(script), 'data-cache-size', '--device', str(fifo)], env=env, capture_output=True, text=True, timeout=4)
            self.assertEqual(p.returncode, 124, p.stderr+p.stdout)
            self.assertTrue(json.loads(p.stdout)['timed_out'])
            marker = Path(td)/'keep.output'; marker.write_text('keep')
            p = subprocess.run(['bash', str(IOCTL.parent/'reset_fs.sh'), '--device', '/missing'], env=dict(env, SPEC_DEV_DIR=td), capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0); self.assertEqual(marker.read_text(), 'keep')
            self.assertIn('--allow-mutation', p.stdout)

    def test_supported_read_commands_and_collection_wrapper_use_live_cli(self):
        commands = {'all-metrics': 2000, 'tree-cache-size': 2400, 'metadata-cache-size': 2500,
                    'cache-tables': 200, 'operation-count': 300, 'operation-time': 400,
                    'cache-hits': 500, 'cache-misses': 600}
        for name, request in commands.items():
            p = subprocess.run([sys.executable, str(IOCTL), name, '--device', '/missing'], capture_output=True, text=True)
            result = json.loads(p.stdout)
            self.assertEqual(result.get('request'), request)
            self.assertFalse(result['mutating'])
        p = subprocess.run(['bash', str(IOCTL.parent/'get_all_metrics.sh'), '--device', '/missing'], capture_output=True, text=True)
        self.assertEqual(json.loads(p.stdout)['request'], 2000)
        self.assertNotEqual(p.returncode, 0)

if __name__ == '__main__': unittest.main()
