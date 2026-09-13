import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ProductionProfilerWiringTest(unittest.TestCase):
    def test_recursive_rejections_complete_cache_metrics_for_every_callback(self):
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        callbacks = {
            "getattr": ("static int cu_fuse_getattr(", "static int cu_fuse_read(", "metadata"),
            "read": ("static int cu_fuse_read(", "static int\ncu_fuse_readdir", "data"),
            "readdir": ("static int\ncu_fuse_readdir", "#define IOCTL_GET_FS_STREAM", "tree"),
            "ioctl": ("static int cu_fuse_ioctl(", "static void start_thallium_engine", "bypass"),
            "readlink": ("static int cu_fuse_readlink(", "#ifdef HAVE_SETXATTR", "bypass"),
        }
        self.assertNotIn("#define CHECK_RECURSIVE", fuse)
        for operation, (begin, end, source) in callbacks.items():
            with self.subTest(operation=operation):
                callback = fuse[fuse.index(begin):fuse.index(end, fuse.index(begin))]
                start = callback.index("Metric::start_cache_operation")
                rejection = callback.index("Util::is_recursive_path_string")
                completion = callback.index("Metric::stop_cache_operation", rejection)
                self.assertLess(start, rejection)
                self.assertLess(rejection, completion)
                completed = callback[completion:]
                self.assertIn(f"OperationFunction::{operation}", completed)
                self.assertIn("OperationResult::neg", completed)
                self.assertIn(f"ProfileSource::{source}", completed)
                self.assertIn("-ENOENT", completed)

    def test_runtime_links_and_metric_completion_feeds_profiler_core(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        metrics = (ROOT / "src/metric/metrics.cpp").read_text()
        profiling = (ROOT / "src/metric/profiling.cpp").read_text()
        self.assertIn("src/metric/profiler_core.cpp", cmake)
        self.assertIn("Operations::complete", metrics)
        self.assertNotIn("Operations::inc_operation_timer", metrics)
        self.assertIn("Operations::snapshot", profiling)
        self.assertNotIn("Operations::get_operation_count", profiling)
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        self.assertIn("ProfileSource::metadata", fuse)
        self.assertIn("ProfileSource::data", fuse)
        self.assertIn("ProfileSource::tree", fuse)
        self.assertNotIn("CacheEvent::md_cache_event_table", fuse)

    def test_runtime_metric_completion_is_configuration_gated(self):
        metrics = (ROOT / "src/metric/metrics.cpp").read_text()
        operations = (ROOT / "src/metric/operations.cpp").read_text()
        self.assertNotIn("COLLECT_METRICS", metrics)
        self.assertIn("if(!enabled()) return;", operations)

    def test_ioctl_stream_failure_returns_same_negative_status_to_metrics(self):
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        macro = fuse[fuse.index("#define IOCTL_GET_FS_STREAM"):fuse.index("static int cu_fuse_ioctl")]
        self.assertIn("negative_errno_or_eio", macro)
        self.assertIn("Metric::stop_cache_operation", macro)
        self.assertIn("OperationResult::neg", macro)
        self.assertNotIn("Constants::fs_operation_error", macro)

    def test_read_fault_paths_are_negative_and_share_cache_admission_rule(self):
        provider = (ROOT / "src/copper/server_local_cache_provider.cpp").read_text()
        start = provider.index("void ServerLocalCacheProvider::rpcRead(")
        end = provider.index("void ServerLocalCacheProvider::rpcReaddir(", start)
        read = provider[start:end]
        self.assertGreaterEqual(read.count("std::make_pair(-EIO"), 4)
        self.assertIn("if(!cached && CopperError::is_success(read_response.first))", read)
        self.assertNotIn("-Constants::", read)

    def test_every_registered_fuse_callback_is_metric_wrapped(self):
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        self.assertIn("FUSE_METRIC_CALLBACK", fuse)
        self.assertNotIn(".write_buf = nullptr", fuse)
        self.assertIn(".read_buf = nullptr", fuse)
        self.assertNotIn("cu_fuse_read_buf", fuse)
        self.assertIn("faccessat", fuse)
        destroy_complete = fuse.index("Metric::stop_operation(OperationFunction::destroy")
        final_report = fuse.index("Util::log_profiling_metrics(Constants::output_filename_path);", destroy_complete)
        self.assertLess(destroy_complete, final_report)

    def test_ttl_state_is_owned_only_by_profiler_core(self):
        header = (ROOT / "src/metric/profiling.h").read_text()
        core = (ROOT / "src/metric/profiler_core.h").read_text()
        self.assertNotIn("md_ttl_event_table", header)
        self.assertIn("record_ttl", core)

    def test_production_report_api_exposes_failure(self):
        header = (ROOT / "src/metric/profiling.h").read_text()
        self.assertRegex(header, r"static\s+ReportResult\s+write_reports")

    def test_read_rejects_negative_offset_before_cache_or_size_arithmetic(self):
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        start = fuse.index("static int cu_fuse_read(")
        end = fuse.index("static int\ncu_fuse_readdir", start)
        callback = fuse[start:end]
        metric_start = callback.index("Metric::start_cache_operation")
        rejection = callback.index("read_offset_result(offset)")
        metric_stop = callback.index("Metric::stop_cache_operation", rejection)
        self.assertLess(metric_start, rejection)
        self.assertLess(rejection, metric_stop)
        self.assertLess(rejection, callback.index("md_cache_table.get"))
        self.assertLess(rejection, callback.index("static_cast<size_t>(offset)"))

    def test_getattr_rpc_population_is_accounted_as_cache_miss(self):
        fuse = (ROOT / "src/fs/cu_fuse.cpp").read_text()
        start = fuse.index("static int cu_fuse_getattr(")
        end = fuse.index("static int cu_fuse_read(", start)
        callback = fuse[start:end]
        self.assertIn("OperationResult result = OperationResult::cache_hit", callback)
        rpc = callback.index("rpc_lstat.on")
        miss = callback.index("result = OperationResult::cache_miss", rpc)
        stop = callback.index("Metric::stop_cache_operation", miss)
        self.assertLess(rpc, miss)
        self.assertLess(miss, stop)
        self.assertIn("OperationFunction::getattr, result,", callback[stop:])


if __name__ == "__main__":
    unittest.main()