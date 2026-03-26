# Copper 1.0 Release Notes

Copper 1.0 is the first production-oriented consolidation of the current
read-mostly caching, profiling, startup-scaling, and documentation work.

## Release Highlights

- Introduces a production-oriented Copper tree with runtime, profiling, tests, and documentation organized together.
- Preserves Copper's read-only cooperative caching model for scalable metadata and data reuse on large HPC systems.
- Keeps `getattr`, `read`, and `readdir` as the primary distributed runtime paths.
- Keeps `open`, `release`, and `opendir` as compatibility stubs for read-mostly startup workflows.
- Supports user-facing log levels `0-5`, with `0` meaning no normal runtime logging and `5` meaning most logging.
- Retains a configurable exact-path metadata `ENOENT` TTL through `-md_enoent_ttl_ms` and wrapper option `-E`.
- Adds prepared job-local address-book generation before the full `cu_fuse` launch.
- Supports `facility` address-book mode for filtering a site-maintained address book down to the active allocation.
- Supports `discover` address-book mode for live endpoint discovery from the current allocation.
- Builds and ships the `list_cxi_hsn_thallium` helper as a normal production build artifact.
- Keeps the address-book discovery helper source inside the production tree under `copper-tests/copper-get-addressbook/`.
- Preserves a filtered runtime address book in `logs/copper_address_book.txt`.
- Preserves a provenance artifact in `logs/copper_address_book_full_output.txt` for address-book preparation.
- Adds runtime support for `-prefiltered_address_book` so all ranks can consume a prepared smaller address-book file directly.
- Removes the need for every rank to reparse the large facility address book during startup.
- Keeps parent-readiness checking before the first forwarded parent RPC.
- Caches parent readiness after the first successful probe so repeated forwarded calls do not keep re-handshaking.
- Retains compact startup timing signals at normal visibility for production operations.
- Emits `provider registration completed after <us>` as a standard startup timing line.
- Emits `first successful parent rpc_... completed after <us> since provider startup` as a standard readiness timing line.
- Retains profiling as a feature independent of normal runtime log verbosity.
- Supports aggregate profiling through `-profile_metrics` and wrapper option `-P`.
- Supports top-path profiling through `-profile_top_n <N>` and wrapper option `-N <N>`.
- Supports full-path profiling through `-profile_paths_full` and wrapper option `-A`.
- Supports periodic profiling snapshots through `-profile_snapshot_interval_s <seconds>` and wrapper option `-I <seconds>`.
- Writes per-rank profiling summaries to `profiling/final/`.
- Writes tagged per-rank profiling snapshots such as `pre-destroy` to `profiling/<snapshot-tag>/`.
- Supports cluster-level postprocessing through `scripts/aggregate_profiling.py`.
- Keeps profiling outputs in structured files including `profiling_summary.md`, `profiling_operations.csv`, `profiling_aggregate.csv`, and `profiling_top_paths.csv`.
- Records metadata TTL behavior in profiling outputs, including stores, serves, expires, and clears.
- Includes self-contained `copper-tests` build wiring under the production tree.
- Adds local build helpers and run scripts for the address-book discovery helper.
- Includes Slurm and PBS helper scripts for the address-book collection tooling.
- Documents Frontier and Aurora platform considerations in the production docs set.
- Keeps deep-debugging guidance for symbol-rich builds, core files, and communication-layer diagnostics.
- Documents operational production guidance for logs, address-book provenance, and Conda activation through Copper.
- Documents scaling behavior and startup design in a dedicated registration-and-scaling page.
- Documents profiling behavior and output interpretation in dedicated profiling pages.
- Documents FUSE `max_read` observations and limitations as technical reference material.
- Documents the HSN-to-CXI address-structure study as an engineering reference page.
- Documents the metadata false-`ENOENT` investigation and its TTL implications.
- Consolidates the maintained docs set under `docs/source/` in `.rst` format with a production-style table of contents.
- Rewrites the top-level `README.md` into a neutral product-facing overview instead of a branch-merge narrative.
- Includes an evaluation report index that points to the historical validation reports and run directories used to inform this release.

## Evaluation and Validation Included with This Release

- Includes documented startup/readiness scaling analysis from the `after-reg-scale-fix-runs` evaluation series.
- Includes documented profiling evaluation from the `after-fix-runs/0_test_2nodes/version4-profiling` run series.
- Includes final-exp validation scripts under `final-exp-test/` for basic import, profiling, full-path profiling, top-path profiling, and discover-mode testing.
- Includes prepared-address-book runtime validation in the final-exp test harness.
- Includes retained production timing signals specifically chosen for large-scale runtime validation without restoring heavy debug logging.

## Documentation Included with This Release

- Includes `overview_and_best_practices.rst` for operational scope and supported workflow guidance.
- Includes `building_and_running.rst` for build flow, runtime model, address-book modes, and profiling controls.
- Includes `building_and_platforms.rst` for Frontier and Aurora platform notes.
- Includes `profiling_and_metrics.rst` for profiling behavior and usage guidance.
- Includes `profiling_metrics.rst` for profiling output reference and terminology.
- Includes `registration_and_scaling.rst` for startup, readiness, and address-book scaling behavior.
- Includes `production_notes.rst` for production logging and provenance guidance.
- Includes `deep_debugging.rst` for symbol-rich builds, core files, and transport-layer debugging.
- Includes `evaluation_reports_index.rst` for the historical run and report index.

## Operational Notes

- Copper 1.0 is intended for read-mostly startup-heavy workloads rather than general read-write filesystem semantics.
- Normal operational visibility is intentionally kept compact; the retained startup timing lines are the main production readiness signals.
- Expected missing-path Python and Conda probes are now treated as routine debug-scale events rather than production error noise where appropriate.
- Profiling can remain enabled even when runtime logs are kept quiet.
