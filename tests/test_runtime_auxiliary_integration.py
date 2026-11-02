"""Exercise production identity capture with the real shutdown verifier."""
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]

class RuntimeAuxiliaryIntegration(unittest.TestCase):
    def test_actual_metrics_writer_publishes_verified_cache_generation(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            # Only libfuse's enum is needed by Util; real cache/report code is linked.
            (directory/'fuse.h').write_text('#pragma once\nenum fuse_fill_dir_flags { FUSE_FILL_DIR_PLUS = 2 };\n')
            source = directory/'writer.cpp'
            source.write_text('''#include "fs/util.h"
#include <iostream>
int main(int argc, char** argv) {
 Constants::log_output_dir = argv[1]; Constants::my_hostname = "hostA";
 Constants::pid = std::to_string(getpid());
 CacheTables::data_cache_table.put_force("/data", std::vector<std::byte>(7));
 CacheTables::tree_cache_table.put_force("/tree", std::vector<std::string>{"abc"});
 CacheTables::md_cache_table.put_force("/metadata", CuStat{});
 auto result = Util::log_all_metrics("ignored");
 if(!result.success) { std::cerr << result.error; return 1; }
 CacheTables::data_cache_table.clear();
 auto final = Util::log_profiling_metrics("ignored");
 if(!final.success) { std::cerr << final.error; return 2; }
 return 0;
}''')
            fuse = (ROOT/'src/fs/cu_fuse.cpp').read_text()
            callback = fuse[fuse.index('#define IOCTL_GET_FS_STREAM'):fuse.index('// static void start_thallium_engine')]
            support = '''#include "metric/metrics.h"
#include "metric/ioctl_event.h"
#include "metric/profiling.h"
#include "fs/error_status.h"
#define COPPER_OPERATION_LOG_TAG "operation"
struct fuse_file_info {};
tp Metric::start_operation(OperationFunction) { return {}; }
std::pair<std::string,tp> Metric::start_cache_operation(OperationFunction, const char* path) { return {Util::rel_to_abs_path(path), {}}; }
int Metric::stop_operation(OperationFunction, tp, int result) { return result; }
int Metric::stop_cache_operation(OperationFunction, OperationResult, ProfileSource, std::string&, tp, int result) { return result; }
'''
            writer = source.read_text().replace('int main(', support+callback+'\nint main(')
            writer = writer.replace('if(!result.success) { std::cerr << result.error; return 1; }', '''if(!result.success) {
 Constants::view_path = "/tmp/mount";
 int status = cu_fuse_ioctl("/.spec_ioctl_file", Constants::ioctl_log_all_metrics, nullptr, nullptr, 0, nullptr);
 if(status >= 0) { std::cerr << "ioctl falsely succeeded after report failure"; return 0; }
 std::cerr << result.error; return 1;
 }''')
            source.write_text(writer)
            binary = directory/'writer'
            sources = ['fs/util.cpp', 'fs/cu_stat.cpp', 'cache/data_cache_table.cpp', 'cache/tree_cache_table.cpp',
                       'cache/md_cache_table.cpp', 'metric/profiler_core.cpp', 'metric/operations.cpp',
                       'metric/cache_event.cpp', 'metric/ioctl_event.cpp', 'metric/profiling.cpp']
            compiler = os.environ.get('CXX', 'g++')
            compile_result = subprocess.run([compiler, '-std=c++20', '-pthread', '-I'+str(directory), '-I'+str(ROOT/'src'),
                            str(source), *[str(ROOT/'src'/s) for s in sources], '-o', str(binary)], capture_output=True, text=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            env = dict(os.environ, COPPER_RUN_ID='cache-run', COPPER_DAEMON_GENERATION='generation')
            output = directory/'output'
            result = subprocess.run([str(binary), str(output)], env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            manifests = list(output.rglob('cache_snapshot.json'))
            self.assertEqual(len(manifests), 2)
            summary = subprocess.run(['python3', str(ROOT/'scripts/get_copper_stats_ioctl/summarize_cache_usage.py'),
                         str(output), '--run-id', 'cache-run', '--expected-host', 'hostA', '--json'], capture_output=True, text=True)
            self.assertEqual(summary.returncode, 0, summary.stdout+summary.stderr)
            report = json.loads(summary.stdout)
            self.assertEqual(report['verification'], 'manifest-verified')
            self.assertEqual(report['tables']['data']['used_bytes'], 0)
            self.assertEqual(report['tables']['tree']['used_bytes'], 3)
            self.assertEqual(report['tables']['metadata']['used_bytes'], 144)
            self.assertEqual(report['selected']['hostA']['sequence'], 2)
            import hashlib
            for path in manifests:
                item = json.loads(path.read_text())
                self.assertEqual(item['capture_semantics'], 'per-table-locked-copy')
                for table in item['tables'].values():
                    self.assertEqual(hashlib.sha256((path.parent/table['path']).read_bytes()).hexdigest(), table['sha256'])
            bad = directory/'not-directory'; bad.write_text('blocked')
            failed = subprocess.run([str(binary), str(bad)], env=env, capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)

    def test_captured_daemon_identity_is_accepted_by_shutdown(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp); directory.chmod(0o700)
            binary = directory / 'daemon'
            compiler = os.environ.get('CXX', 'g++')
            subprocess.run([compiler, '-std=c++20', '-I'+str(ROOT/'src'), str(ROOT/'tests/test_daemon_identity.cpp'), '-o', str(binary)], check=True)
            process = subprocess.Popen([str(binary), str(directory), '-vpath', '/tmp/copper-integration-mount'], stdout=subprocess.PIPE, text=True)
            unrelated = subprocess.Popen(['sleep', '30'])
            try:
                self.assertEqual(int(process.stdout.readline()), process.pid)
                record = json.loads((directory/(socket.gethostname()+'.json')).read_text())
                spec = importlib.util.spec_from_file_location('shutdown', ROOT/'scripts/auxiliary_shutdown.py')
                shutdown = importlib.util.module_from_spec(spec); spec.loader.exec_module(shutdown)
                self.assertIsNone(shutdown.verify_identity(record))
                manifest = directory/'daemon-identity.json'
                manifest.write_text(json.dumps({'schema_version':1, 'job_id':'job.exact', 'expected_hosts':[socket.gethostname()], 'nodes':{socket.gethostname():record}})); manifest.chmod(0o600)
                result = subprocess.run(['python3', str(ROOT/'scripts/auxiliary_shutdown.py'), '--manifest', str(manifest), '--job-id', 'job.exact', '--local', '--timeout', '1'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertTrue(json.loads(result.stdout)['process_exited'])
                self.assertIsNone(unrelated.poll())
            finally:
                process.terminate() if process.poll() is None else None
                process.wait(); process.stdout.close()
                unrelated.terminate(); unrelated.wait()

import test_launch_copper_aurora as aurora_tests

class FrontierIdentityIntegration(aurora_tests.AuroraLauncherTest):
    def setUp(self):
        super().setUp()
        self._write_executable(self.fakebin/'scontrol', '#!/bin/bash\nprintf "node-a\\nnode-b\\n"\n')
        self._write_executable(self.fakebin/'srun', f'''#!/bin/bash
printf '%s\\n' "$*" >> {self._q(self.clush_log)}
[[ -e {self._q(self.fail_launch)} && "${{@: -1}}" == *cu_fuse* ]] && exit 42
while [[ "$1" == -* ]]; do
  case "$1" in --*=*) shift;; *) shift; [[ "$1" == -* ]] || shift;; esac
done
exec "$@"
''')
        self._write_executable(self.fakebin/'taskset', '#!/bin/bash\nshift 2\nexec "$@"\n')

    def run_launcher(self, *extra):
        env = os.environ.copy()
        env.update(COPPER_ROOT=str(self.copper_root), SLURM_JOB_ID='123', SLURM_JOB_NODELIST='node-a,node-b',
                   SLURM_NNODES='2', USER='tester', PATH=f"{self.fakebin}:{env['PATH']}",
                   LD_LIBRARY_PATH=str(self.work/'runtime libs'), FI_PROVIDER='cxi;safe value',
                   MARGO_CONFIG='value with spaces;$(false)')
        (self.build/'olcf_frontier_copper_addressbook.txt').write_text((self.build/'alcf_aurora_copper_addressbook.txt').read_text())
        return subprocess.run(['bash', str(ROOT/'scripts/launch_copper_frontier.sh'), '-s', '0', '-d', str(self.work/'logs'),
                               '-v', str(self.mount), '-R', '1', '-B', '0', *extra], env=env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)

    def test_ready_launch_publishes_private_exact_job_manifest(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stdout)
        manifest = self.work/'logs'/'123'/'daemon-identity.json'
        self.assertTrue(manifest.exists(), 'Frontier ready launch did not publish identities')
        data = json.loads(manifest.read_text())
        self.assertEqual(data['job_id'], '123')
        self.assertEqual(data['expected_hosts'], ['node-a', 'node-b'])
        self.assertEqual(manifest.stat().st_mode & 0o777, 0o600)

if __name__ == '__main__': unittest.main()
