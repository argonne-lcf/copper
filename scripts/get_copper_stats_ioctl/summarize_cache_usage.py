#!/usr/bin/env python3

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path


TOTAL_BYTES_RE = re.compile(r"^Total bytes:\s*(\d+)\s*$")
TOTAL_FILES_RE = re.compile(r"^total number of files:\s*(\d+)\s*$")
TOTAL_ENTRIES_RE = re.compile(r"^total number of entries:\s*(\d+)\s*$")

TABLE_FILES = {
    "data": "data_cache_size.output",
    "tree": "tree_cache_size.output",
    "metadata": "md_cache_size.output",
}


def human_bytes(num_bytes):
    value = float(num_bytes)
    for unit in ["B", "KiB", "MiB", "GiB", "TiB"]:
        if value < 1024.0 or unit == "TiB":
            return f"{value:.2f} {unit}"
        value /= 1024.0
    return f"{num_bytes} B"


def parse_cache_size_file(path):
    total_bytes = None
    total_entries = None

    with path.open("r", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()

            match = TOTAL_BYTES_RE.match(line)
            if match:
                total_bytes = int(match.group(1))
                continue

            match = TOTAL_FILES_RE.match(line)
            if match:
                total_entries = int(match.group(1))
                continue

            match = TOTAL_ENTRIES_RE.match(line)
            if match:
                total_entries = int(match.group(1))

    if total_bytes is None:
        raise ValueError(f"failed to find 'Total bytes' in {path}")
    if total_entries is None:
        raise ValueError(f"failed to find entry count in {path}")

    return total_bytes, total_entries


def discover_files(root):
    discovered = defaultdict(list)
    for path in root.rglob("*.output"):
        for table_name, filename in TABLE_FILES.items():
            if path.name == filename or path.name.endswith("-" + filename):
                discovered[table_name].append(path)
                break
    return discovered


def summarize_root(root):
    discovered = discover_files(root)
    summary = {}

    for table_name, filename in TABLE_FILES.items():
        table_files = sorted(discovered.get(table_name, []))
        table_total_bytes = 0
        table_total_entries = 0
        parsed_files = 0

        for path in table_files:
            total_bytes, total_entries = parse_cache_size_file(path)
            table_total_bytes += total_bytes
            table_total_entries += total_entries
            parsed_files += 1

        summary[table_name] = {
            "filename": filename,
            "files_found": parsed_files,
            "used_bytes": table_total_bytes,
            "entry_count": table_total_entries,
        }

    return summary


def render_summary_lines(root, summary):
    combined_used_bytes = sum(item["used_bytes"] for item in summary.values())
    combined_entries = sum(item["entry_count"] for item in summary.values())

    headers = ["Table", "Files Found", "Used Bytes", "Used Human", "Entries"]
    rows = []
    for table_name in ["data", "tree", "metadata"]:
        item = summary[table_name]
        rows.append([
            table_name,
            str(item["files_found"]),
            str(item["used_bytes"]),
            human_bytes(item["used_bytes"]),
            str(item["entry_count"]),
        ])
    rows.append(["combined", "-", str(combined_used_bytes), human_bytes(combined_used_bytes), str(combined_entries)])

    widths = [len(header) for header in headers]
    for row in rows:
        for index, value in enumerate(row):
            widths[index] = max(widths[index], len(value))

    def make_row(values):
        return "| " + " | ".join(value.ljust(widths[index]) for index, value in enumerate(values)) + " |"

    align = []
    for index, width in enumerate(widths):
        if index == 0:
            align.append("-" * width)
        else:
            align.append("-" * max(width - 1, 1) + ":")

    lines = [
        f"cache usage summary root: {root}",
        "",
        make_row(headers),
        "| " + " | ".join(align) + " |",
    ]
    lines.extend(make_row(row) for row in rows)
    lines.extend([
        "",
        "note: Copper currently reports current table occupancy, not a fixed total cache budget or remaining available bytes.",
    ])
    return lines


def print_summary(root, summary):
    for line in render_summary_lines(root, summary):
        print(line)


def write_csv(path, summary):
    combined_used_bytes = sum(item["used_bytes"] for item in summary.values())
    combined_entries = sum(item["entry_count"] for item in summary.values())

    with open(path, "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["table", "files_found", "used_bytes", "used_human", "entries"])
        for table_name in ["data", "tree", "metadata"]:
            item = summary[table_name]
            writer.writerow(
                [
                    table_name,
                    item["files_found"],
                    item["used_bytes"],
                    human_bytes(item["used_bytes"]),
                    item["entry_count"],
                ]
            )
        writer.writerow(["combined", "", combined_used_bytes, human_bytes(combined_used_bytes), combined_entries])


def main():
    parser = argparse.ArgumentParser(
        description="Summarize Copper cache table usage from *_cache_size.output files."
    )
    parser.add_argument(
        "metrics_root",
        help="directory that contains cache size output files directly or under subdirectories",
    )
    parser.add_argument(
        "--csv",
        default=None,
        help="optional path to write a CSV summary",
    )
    args = parser.parse_args()

    root = Path(args.metrics_root).resolve()
    if not root.exists():
        raise SystemExit(f"metrics root does not exist: {root}")

    summary = summarize_root(root)
    print_summary(root, summary)

    if args.csv:
        write_csv(args.csv, summary)
        print(f"wrote csv summary: {args.csv}", file=sys.stderr)


if __name__ == "__main__":
    main()
