#!/usr/bin/env python3

import argparse
import os
from pathlib import Path

from aggregate_profiling import infer_conda_roots, read_observed_paths


def infer_home_child_roots(observed_paths, home_root):
    if not home_root:
        return []

    normalized_home = os.path.normpath(home_root)
    child_roots = set()
    saw_home_root = False

    for path in observed_paths:
        if path == normalized_home:
            saw_home_root = True
            continue
        if not path.startswith(normalized_home + os.sep):
            continue
        rel = os.path.relpath(path, normalized_home)
        if rel == ".":
            saw_home_root = True
            continue
        first = rel.split(os.sep, 1)[0]
        child_roots.add(os.path.join(normalized_home, first))

    roots = sorted(child_roots)
    if saw_home_root:
        roots.insert(0, normalized_home)
    return roots


def minimize_roots(roots):
    minimized = []
    normalized_roots = []
    for root in roots:
        if not root:
            continue
        normalized_roots.append(os.path.normpath(root))

    for root in sorted(set(normalized_roots), key=lambda value: (len(value), value)):
        if any(root == kept or root.startswith(kept + os.sep) for kept in minimized):
            continue
        minimized.append(root)
    return minimized


def path_is_under_any_root(path, roots):
    normalized_path = os.path.normpath(path)
    for root in roots:
        if normalized_path == root or normalized_path.startswith(root + os.sep):
            return True
    return False


def iter_existing_paths(root):
    normalized_root = os.path.normpath(root)
    if not os.path.exists(normalized_root):
        return

    for dirpath, _, filenames in os.walk(normalized_root):
        yield os.path.normpath(dirpath)
        for filename in filenames:
            yield os.path.normpath(os.path.join(dirpath, filename))


def classify_entries(paths):
    counts = {
        "total": 0,
        "files": 0,
        "dirs": 0,
        "missing": 0,
        "other": 0,
    }
    for path in paths:
        counts["total"] += 1
        if os.path.isfile(path):
            counts["files"] += 1
        elif os.path.isdir(path):
            counts["dirs"] += 1
        elif not os.path.exists(path):
            counts["missing"] += 1
        else:
            counts["other"] += 1
    return counts


def write_list(path, entries):
    with open(path, "w") as f:
        for entry in entries:
            f.write(f"{entry}\n")


def build_counts_rows(output_names, output_sets):
    rows = []
    for label, filename in output_names:
        counts = classify_entries(output_sets[filename])
        rows.append({
            "label": label,
            "filename": filename,
            "counts": counts,
        })
    return rows


def format_percent(numerator, denominator):
    if denominator == 0:
        return "0.00%"
    return f"{(100.0 * numerator / denominator):.2f}%"


def render_summary_md(log_dir, tables_dir, roots, output_names, output_sets, output_dir):
    rows = build_counts_rows(output_names, output_sets)
    summary_path = os.path.join(output_dir, "paths_summary.md")

    with open(summary_path, "w") as f:
        f.write("# Path Usage Analysis\n\n")
        f.write(f"- Log directory: `{log_dir}`\n")
        f.write(f"- Raw full-path input directory: `{tables_dir}`\n")
        f.write(f"- Output directory: `{output_dir}`\n\n")

        f.write("## Selected roots\n\n")
        if roots:
            for root in roots:
                f.write(f"- `{root}`\n")
        else:
            f.write("- none inferred\n")
        f.write("\n")

        f.write("## In Simple Terms\n\n")
        f.write(
            "- `all_possible_existing_paths.txt`: the full existing path universe under the selected roots for this analysis.\n"
        )
        f.write(
            "- `used_paths_existing.txt`: paths that `import torch` actually touched and that really existed.\n"
        )
        f.write(
            "- `same_run_candidate_not_observed_existing_paths.txt`: paths that existed in the same area but did not show up in this run. This is the \"maybe safe to prune for the exact same rerun\" list.\n"
        )
        f.write(
            "- `missing_probe_paths.txt`: paths that Python or native loaders checked for, but that were already not there. These are usually probe misses, not deletion candidates.\n\n"
        )

        f.write("The best short mental model is:\n\n")
        f.write("- `all_possible` = the local universe under the selected roots\n")
        f.write("- `used` = keep\n")
        f.write("- `candidate_not_observed` = prune candidates for the same rerun\n")
        f.write("- `missing_probe` = normal misses that were already absent\n\n")

        f.write("## Counts Table\n\n")
        f.write("| Meaning | File | Total Paths | Files | Directories | Missing |\n")
        f.write("|---|---|---:|---:|---:|---:|\n")
        for row in rows:
            counts = row["counts"]
            f.write(
                f"| {row['label']} | `{row['filename']}` | "
                f"{counts['total']} | {counts['files']} | {counts['dirs']} | {counts['missing']} |\n"
            )
        f.write("\n")

        all_existing = output_sets["all_possible_existing_paths.txt"]
        used_existing = output_sets["used_paths_existing.txt"]
        missing_probe = output_sets["missing_probe_paths.txt"]
        candidate_not_observed = output_sets["same_run_candidate_not_observed_existing_paths.txt"]
        observed_total = len(used_existing) + len(missing_probe)
        all_existing_counts = classify_entries(all_existing)
        used_existing_counts = classify_entries(used_existing)

        f.write("## Interpretation Notes\n\n")
        f.write(
            f"- Observed paths under the selected roots in this run: `{observed_total}`\n"
        )
        f.write(
            f"- Existing observed paths: `{len(used_existing)}`\n"
        )
        f.write(
            f"- Missing probe paths: `{len(missing_probe)}`\n"
        )
        f.write(
            f"- Existing paths not observed in this run: `{len(candidate_not_observed)}`\n"
        )
        f.write(
            f"- All existing paths under the selected roots: `{len(all_existing)}`\n\n"
        )
        f.write("## Coverage Estimates\n\n")
        f.write(
            f"- Observed files: `{used_existing_counts['files']}` of `{all_existing_counts['files']}` "
            f"(`{format_percent(used_existing_counts['files'], all_existing_counts['files'])}`)\n"
        )
        f.write(
            f"- Observed directories: `{used_existing_counts['dirs']}` of `{all_existing_counts['dirs']}` "
            f"(`{format_percent(used_existing_counts['dirs'], all_existing_counts['dirs'])}`)\n\n"
        )
        f.write(
            "This is not a proof of safe deletion. It is only a same-app, same-node-count, same-config candidate list. A later run can still need a path that did not appear here.\n"
        )

    return summary_path


def render_roots_note(log_dir, roots, output_sets, output_dir):
    note_path = os.path.join(output_dir, "roots_and_counts.txt")
    used_existing = output_sets["used_paths_existing.txt"]
    missing_probe = output_sets["missing_probe_paths.txt"]
    candidate_not_observed = output_sets["same_run_candidate_not_observed_existing_paths.txt"]
    all_existing = output_sets["all_possible_existing_paths.txt"]
    all_existing_counts = classify_entries(all_existing)
    used_existing_counts = classify_entries(used_existing)

    with open(note_path, "w") as f:
        f.write(f"Path list generation for {os.path.basename(log_dir)}\n\n")
        f.write("Method:\n")
        f.write("- Used paths were taken from the raw final full-path outputs under tables/final/*.output.\n")
        f.write("- Selected roots were inferred from observed paths, mainly the active conda environment and any observed first-level home children.\n")
        f.write("- Additional roots may be supplied with --usage-root.\n")
        f.write("- Candidate not observed paths are existing files/directories under those same selected roots that did not appear in the final full-path outputs.\n")
        f.write("- This is not a proof of safe deletion. It is only a same-app, same-node-count, same-config candidate list.\n\n")
        f.write("Selected roots:\n")
        if roots:
            for root in roots:
                f.write(f"- {root}\n")
        else:
            f.write("- none inferred\n")
        f.write("\n")
        f.write(f"All existing paths under selected roots: {len(all_existing)}\n")
        f.write(f"Observed existing paths under selected roots: {len(used_existing)}\n")
        f.write(f"Observed missing probe paths under selected roots: {len(missing_probe)}\n")
        f.write(f"Observed paths under selected roots: {len(used_existing) + len(missing_probe)}\n")
        f.write(f"Candidate existing paths not observed in final outputs: {len(candidate_not_observed)}\n\n")
        f.write("Coverage estimates:\n")
        f.write(
            f"- Observed files: {used_existing_counts['files']} of {all_existing_counts['files']} "
            f"({format_percent(used_existing_counts['files'], all_existing_counts['files'])})\n"
        )
        f.write(
            f"- Observed directories: {used_existing_counts['dirs']} of {all_existing_counts['dirs']} "
            f"({format_percent(used_existing_counts['dirs'], all_existing_counts['dirs'])})\n\n"
        )
        f.write("File meanings:\n")
        f.write("- all_possible_existing_paths.txt: all existing files/directories under the selected roots.\n")
        f.write("- used_paths_existing.txt: all observed existing files/directories under the selected roots.\n")
        f.write("- missing_probe_paths.txt: paths that were probed but did not exist during the run.\n")
        f.write("- same_run_candidate_not_observed_existing_paths.txt: existing paths under the selected roots that were not seen in the final full-path outputs.\n")
        f.write("- roots_and_counts.txt: this note file.\n")
        f.write("- paths_summary.md: Markdown summary with a plain-language explanation and counts table.\n")

    return note_path


def main():
    parser = argparse.ArgumentParser(
        description="Create post-run path usage lists from Copper full-path profiling outputs."
    )
    parser.add_argument("log_dir", help="Copper run directory, typically the <jobid> log directory")
    parser.add_argument(
        "--output-dir",
        default=None,
        help="directory for generated txt/md outputs; defaults to <log_dir>/paths_dir",
    )
    parser.add_argument(
        "--usage-root",
        action="append",
        default=[],
        help="additional filesystem root to include in the analysis; can be passed multiple times",
    )
    parser.add_argument(
        "--skip-home-inference",
        action="store_true",
        help="do not infer first-level home roots from observed paths",
    )
    args = parser.parse_args()

    log_dir = os.path.abspath(args.log_dir)
    tables_final_dir = os.path.join(log_dir, "tables", "final")
    search_tables_dir = tables_final_dir if os.path.isdir(tables_final_dir) else log_dir
    output_dir = os.path.abspath(args.output_dir or os.path.join(log_dir, "paths_dir"))
    os.makedirs(output_dir, exist_ok=True)

    observed_paths = read_observed_paths(search_tables_dir)

    selected_roots = list(args.usage_root)
    selected_roots.extend(infer_conda_roots(observed_paths))
    if not args.skip_home_inference:
        selected_roots.extend(infer_home_child_roots(observed_paths, os.path.expanduser("~")))
    selected_roots = minimize_roots(selected_roots)

    observed_under_roots = sorted(
        path for path in observed_paths if path_is_under_any_root(path, selected_roots)
    )
    used_existing = sorted(path for path in observed_under_roots if os.path.exists(path))
    missing_probe = sorted(path for path in observed_under_roots if not os.path.exists(path))

    all_existing_paths = set()
    for root in selected_roots:
        for path in iter_existing_paths(root):
            all_existing_paths.add(path)
    all_existing_paths = sorted(all_existing_paths)

    used_existing_set = set(used_existing)
    candidate_not_observed = sorted(path for path in all_existing_paths if path not in used_existing_set)

    output_sets = {
        "all_possible_existing_paths.txt": all_existing_paths,
        "used_paths_existing.txt": used_existing,
        "same_run_candidate_not_observed_existing_paths.txt": candidate_not_observed,
        "missing_probe_paths.txt": missing_probe,
    }
    output_names = [
        ("All possible existing paths under the selected roots", "all_possible_existing_paths.txt"),
        ("Actually used in this run", "used_paths_existing.txt"),
        ("Not seen in this run, but exists", "same_run_candidate_not_observed_existing_paths.txt"),
        ("Probed but absent", "missing_probe_paths.txt"),
    ]

    for filename, entries in output_sets.items():
        write_list(os.path.join(output_dir, filename), entries)

    note_path = render_roots_note(log_dir, selected_roots, output_sets, output_dir)
    summary_path = render_summary_md(
        log_dir, search_tables_dir, selected_roots, output_names, output_sets, output_dir
    )

    print(f"wrote {os.path.join(output_dir, 'all_possible_existing_paths.txt')}")
    print(f"wrote {os.path.join(output_dir, 'used_paths_existing.txt')}")
    print(
        f"wrote {os.path.join(output_dir, 'same_run_candidate_not_observed_existing_paths.txt')}"
    )
    print(f"wrote {os.path.join(output_dir, 'missing_probe_paths.txt')}")
    print(f"wrote {note_path}")
    print(f"wrote {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
