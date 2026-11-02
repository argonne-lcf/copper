import json, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT/'scripts/get_copper_stats_ioctl/summarize_cache_usage.py'

class CacheTests(unittest.TestCase):
    def run_summary(self, root, *args):
        return subprocess.run([sys.executable, str(SCRIPT), str(root), '--json', *args], capture_output=True, text=True)

    def snapshot(self, root, host, sequence, amount=10, run='r', epoch=0):
        folder = root / f'{host}-{sequence}'; folder.mkdir(exist_ok=True)
        tables = {}
        for kind in ('data', 'tree', 'metadata'):
            p = folder/(kind+'.output')
            p.write_text(f'Total bytes: {amount}\ntotal number of entries: 2\n')
            import hashlib
            tables[kind] = {'path': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
        (folder/'cache_snapshot.json').write_text(json.dumps({'schema_version': 1, 'run_id': run, 'host': host, 'epoch': epoch, 'sequence': sequence, 'tables': tables}))

    def test_latest_single_generation_per_host_and_explicit_coverage(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            self.snapshot(root, 'a', 1, 999); self.snapshot(root, 'a', 2, 10); self.snapshot(root, 'b', 1, 20)
            p = self.run_summary(root, '--expected-host', 'a', '--expected-host', 'b', '--run-id', 'r')
            self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
            result = json.loads(p.stdout)
            self.assertEqual(result['tables']['data']['used_bytes'], 30)
            self.assertEqual(result['coverage']['missing'], [])
            self.assertEqual(result['selected']['a']['sequence'], 2)
            self.assertEqual(result['verification'], 'manifest-verified')

    def test_reject_ambiguous_or_unverified_generation_inputs(self):
        for case in ('duplicate', 'mixed-run', 'mixed-epoch', 'hash', 'missing-expectation', 'unexpected-host', 'escape', 'schema', 'negative-sequence'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as td:
                import shutil
                root = Path(td); self.snapshot(root, 'a', 1)
                path = root/'a-1/cache_snapshot.json'; item = json.loads(path.read_text())
                args = ['--expected-host', 'a', '--run-id', 'r']
                if case == 'duplicate': shutil.copytree(root/'a-1', root/'copy')
                elif case == 'mixed-run': self.snapshot(root, 'a', 2, run='other')
                elif case == 'mixed-epoch': self.snapshot(root, 'a', 2, epoch=1)
                elif case == 'hash': (root/'a-1/data.output').write_text('Total bytes: 100\ntotal number of entries: 2\n')
                elif case == 'missing-expectation': args = []
                elif case == 'unexpected-host': self.snapshot(root, 'b', 1)
                elif case == 'escape': item['tables']['data']['path'] = '../a-1/data.output'
                elif case == 'schema': item['schema_version'] = 2
                elif case == 'negative-sequence': item['sequence'] = -1
                path.write_text(json.dumps(item))
                p = self.run_summary(root, *args)
                self.assertNotEqual(p.returncode, 0, case + p.stdout)
                self.assertFalse(json.loads(p.stdout)['ok'])

    def test_legacy_requires_opt_in_and_never_claims_verified_coverage(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for name in ('data', 'tree', 'md'):
                (root/f'a-{name}_cache_size.output').write_text('Total bytes: 12\ntotal number of entries: 2\n')
            p = self.run_summary(root, '--legacy', '--expected-host', 'a')
            self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
            result = json.loads(p.stdout)
            self.assertEqual(result['verification'], 'legacy-unverified')
            self.assertEqual(result['tables']['data']['used_bytes'], 12)
            nested = root/'duplicate'; nested.mkdir()
            (nested/'a-data_cache_size.output').write_text('Total bytes: 12\ntotal number of entries: 2\n')
            self.assertNotEqual(self.run_summary(root, '--legacy', '--expected-host', 'a').returncode, 0)

    def test_real_legacy_host_pid_filename_is_supported_but_multiple_pids_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for name in ('data', 'tree', 'md'):
                (root/f'node-with-hyphen-123-{name}_cache_size.output').write_text('Total bytes: 12\ntotal number of files: 2\n')
            p = self.run_summary(root, '--legacy', '--expected-host', 'node-with-hyphen')
            self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
            (root/'node-with-hyphen-456-data_cache_size.output').write_text('Total bytes: 12\ntotal number of files: 2\n')
            self.assertNotEqual(self.run_summary(root, '--legacy', '--expected-host', 'node-with-hyphen').returncode, 0)

    def test_legacy_rejects_mixed_pid_tables_for_one_host(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for pid, name in ((123, 'data'), (456, 'tree'), (123, 'md')):
                (root/f'a-{pid}-{name}_cache_size.output').write_text('Total bytes: 12\ntotal number of entries: 2\n')
            p = self.run_summary(root, '--legacy', '--expected-host', 'a')
            self.assertNotEqual(p.returncode, 0)

    def test_csv_preserves_unverified_label_and_coverage(self):
        import csv
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for name in ('data', 'tree', 'md'):
                (root/f'a-{name}_cache_size.output').write_text('Total bytes: 12\ntotal number of entries: 2\n')
            csv_path = root/'summary.csv'
            p = self.run_summary(root, '--legacy', '--expected-host', 'a', '--csv', str(csv_path))
            self.assertEqual(p.returncode, 0)
            with csv_path.open() as stream: rows = list(csv.DictReader(stream))
            self.assertEqual(rows[0].get('verification'), 'legacy-unverified')
            self.assertEqual(rows[0].get('expected_hosts'), 'a')

    def test_wrapper_summarizes_only_and_propagates_incomplete_coverage(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); self.snapshot(root, 'a', 1)
            p = subprocess.run(['bash', str(SCRIPT.parent/'get_cache_usage_summary.sh'), td, '--json', '--run-id', 'r', '--expected-host', 'a', '--expected-host', 'b'], capture_output=True, text=True)
            self.assertNotEqual(p.returncode, 0)
            self.assertEqual(json.loads(p.stdout)['coverage']['missing'], ['b'])

if __name__ == '__main__': unittest.main()
