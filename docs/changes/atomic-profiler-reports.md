# Atomic profiler report bundles

## Problem
Crashes could expose partial reports with no machine-readable provenance.

## Design
Production reports render operations (including latency quantiles/outcomes/errors), bounded path events (including source/outcome/errors), and aggregates from one snapshot. Each file is written to a temporary path and atomically renamed; the manifest is renamed last and records schema, sequence/kind, timestamp, host/pid, mode/config, dropped counts, totals, and files. `Profiling::write_reports` and `Util::log_profiling_metrics` return structured success/error data, and runtime failures are logged.

Host and report-kind values are preserved in manifest metadata but sanitized before use as filename components, preventing separators/control characters from escaping the requested report directory. The offline aggregator also rejects a bundle when manifest path counters disagree with its aggregate CSV.

## Tests
Unit tests verify the manifest, final files, and absence of temporary remnants.

## Limitations
Atomic rename assumes source and destination share a filesystem.
