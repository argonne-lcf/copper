import json, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT/'scripts/path_usage_analysis.py'

class PathTests(unittest.TestCase):
    def run_analysis(self, root, *args):
        return subprocess.run([sys.executable, str(SCRIPT), str(root), *args], capture_output=True, text=True)

    def test_default_reports_observations_without_live_existence_claims(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); tables = root/'tables/final'; tables.mkdir(parents=True)
            (tables/'host.output').write_text('/old/environment/file {count: 1}\n')
            p = self.run_analysis(root, '--usage-root', '/old/environment')
            self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
            self.assertTrue(p.stdout.startswith('{'), 'machine readable path report required')
            result = json.loads(p.stdout)
            self.assertEqual(result['inventory_source'], 'none')
            self.assertEqual(result['historical_existence'], 'unknown')
            self.assertEqual(result['observed_count'], 1)
            self.assertEqual(result['live_walk_performed'], False)
            summary = (root/'paths_dir/paths_summary.md').read_text()
            self.assertNotIn('prune candidates', summary)
            self.assertNotIn('already absent', summary)

    def test_saved_inventory_replays_without_stat_and_disk_mode_matches(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); tables = root/'tables/final'; tables.mkdir(parents=True)
            (tables/'host.output').write_text('/historical/a {count: 1}\n/historical/missing {count: 1}\n/historical/a {count: 2}\n')
            inventory = root/'inventory.jsonl'
            inventory.write_text(json.dumps({'schema_version': 1, 'kind': 'path-inventory', 'captured_at': '2020-01-01T00:00:00Z', 'roots': ['/historical'], 'complete': True})+'\n'+json.dumps({'path': '/historical/a', 'type': 'file'})+'\n'+json.dumps({'path': '/historical/b', 'type': 'file'})+'\n')
            for extra in ([], ['--memory-bounded']):
                p = self.run_analysis(root, '--inventory', str(inventory), *extra)
                self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
                result = json.loads(p.stdout)
                self.assertEqual(result['inventory_source'], 'saved')
                self.assertEqual(result['observed_count'], 2)
                self.assertEqual(result['observed_in_inventory'], 1)
                self.assertEqual(result['inventory_not_observed'], 1)
                self.assertEqual(result['observed_not_in_inventory'], 1)
                self.assertEqual(result['historical_existence'], 'unknown')
                self.assertFalse(result['live_walk_performed'])
                self.assertEqual((root/'paths_dir/inventory_not_observed.txt').read_text(), '/historical/b\n')

    def test_inventory_rejects_invalid_paths_counts_and_incomplete_metadata(self):
        for case in ('relative', 'count', 'outside-roots', 'type', 'metadata'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as td:
                root = Path(td)
                header = {'schema_version': 1, 'kind': 'path-inventory', 'captured_at': '2020-01-01T00:00:00Z', 'roots': ['/a'], 'complete': True, 'entry_count': 1}
                row = {'path': '/a/file', 'type': 'file'}
                if case == 'relative': row['path'] = 'a/file'
                if case == 'count': header['entry_count'] = 99
                if case == 'outside-roots': row['path'] = '/b/file'
                if case == 'type': row['type'] = 'imaginary'
                if case == 'metadata': del header['captured_at']
                inventory = root/'bad.jsonl'; inventory.write_text(json.dumps(header)+'\n'+json.dumps(row)+'\n')
                p = self.run_analysis(root, '--inventory', str(inventory))
                self.assertNotEqual(p.returncode, 0)

    def test_live_walk_is_explicit_bounded_and_saved_for_replay(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); tree = root/'tree'; tree.mkdir()
            for n in range(8): (tree/str(n)).write_text('x')
            (tree/'outside').symlink_to('/tmp', target_is_directory=True)
            inventory = root/'saved.jsonl'
            p = self.run_analysis(root, '--walk', '--usage-root', str(tree), '--max-entries', '3', '--save-inventory', str(inventory), '--memory-bounded')
            self.assertEqual(p.returncode, 1, p.stdout+p.stderr)
            self.assertTrue(inventory.exists(), 'bounded walk should save partial inventory')
            result = json.loads(p.stdout)
            self.assertEqual(result['inventory_source'], 'live')
            self.assertTrue(result['live_walk_performed'])
            self.assertFalse(result['inventory_metadata']['complete'])
            self.assertEqual(result['inventory_metadata']['entry_count'], 3)
            p = self.run_analysis(root, '--walk', '--usage-root', str(tree), '--max-entries', '100', '--save-inventory', str(inventory))
            self.assertEqual(p.returncode, 0, p.stdout+p.stderr)
            result = json.loads(p.stdout)
            self.assertEqual(result['inventory_metadata']['entry_count'], 10)
            self.assertTrue(result['inventory_metadata']['complete'])
            p = self.run_analysis(root, '--inventory', str(inventory), '--memory-bounded')
            self.assertEqual(p.returncode, 0)
            self.assertFalse(json.loads(p.stdout)['live_walk_performed'])
            p = self.run_analysis(root, '--walk')
            self.assertNotEqual(p.returncode, 0)

if __name__ == '__main__': unittest.main()
