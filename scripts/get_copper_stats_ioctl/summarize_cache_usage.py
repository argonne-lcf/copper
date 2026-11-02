#!/usr/bin/env python3
"""Summarize one manifest-committed cache snapshot per expected host."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re

TABLE_FILES = {'data': 'data_cache_size.output', 'tree': 'tree_cache_size.output', 'metadata': 'md_cache_size.output'}


def parse_cache_size_file(path, text=None):
    text = path.read_text() if text is None else text
    size = re.findall(r'^Total bytes:\s*(\d+)\s*$', text, re.M)
    entries = re.findall(r'^total number of (?:files|entries):\s*(\d+)\s*$', text, re.M)
    if len(size) != 1 or len(entries) != 1:
        raise ValueError(f'expected exactly one bytes and entries total in {path}')
    return int(size[0]), int(entries[0])


def summarize_root(root, expected_hosts, run_id):
    if not expected_hosts or len(set(expected_hosts)) != len(expected_hosts) or not run_id:
        raise ValueError('explicit unique expected hosts and run-id are required')
    if not root.is_dir():
        raise ValueError('metrics root is not a directory')
    manifests = []
    seen = set()
    epochs = set()
    for path in root.rglob('cache_snapshot.json'):
        item = json.loads(path.read_text()); item['_path'] = path
        if item['schema_version'] != 1 or item['run_id'] != run_id:
            raise ValueError('unsupported schema or mixed run')
        if item['host'] not in expected_hosts:
            raise ValueError('unexpected host')
        for field in ('epoch', 'sequence'):
            if type(item[field]) is not int or item[field] < 0:
                raise ValueError('invalid generation integer')
        identity = (item['host'], item['epoch'], item['sequence'])
        if identity in seen:
            raise ValueError('duplicate snapshot identity')
        seen.add(identity); epochs.add(item['epoch'])
        if len(epochs) != 1:
            raise ValueError('mixed epochs; select a single generation directory')
        if set(item['tables']) != set(TABLE_FILES):
            raise ValueError('snapshot table coverage mismatch')
        manifests.append(item)
    selected = {}
    for item in manifests:
        host = item['host']
        if host not in selected or item['sequence'] > selected[host]['sequence']:
            selected[host] = item
    tables = {name: {'used_bytes': 0, 'entry_count': 0, 'files_found': 0} for name in TABLE_FILES}
    for host, item in selected.items():
        for name in tables:
            reference = item['tables'][name]
            relative = Path(reference['path'])
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('unsafe snapshot path')
            file = item['_path'].parent / relative
            parent = item['_path'].parent.resolve()
            resolved = file.resolve()
            try:
                resolved.relative_to(parent)
            except ValueError:
                raise ValueError('snapshot path escapes bundle')
            if file.is_symlink():
                raise ValueError('snapshot path escapes bundle')
            content = file.read_bytes()
            if hashlib.sha256(content).hexdigest() != reference['sha256']:
                raise ValueError('snapshot hash mismatch')
            size, entries = parse_cache_size_file(file, content.decode('utf-8'))
            tables[name]['used_bytes'] += size
            tables[name]['entry_count'] += entries
            tables[name]['files_found'] += 1
    missing = sorted(set(expected_hosts) - set(selected))
    return {'ok': not missing, 'verification': 'manifest-verified', 'run_id': run_id,
            'coverage': {'expected': sorted(expected_hosts), 'observed': sorted(selected), 'missing': missing},
            'selected': {h: {k: v for k, v in item.items() if k != '_path'} for h, item in selected.items()}, 'tables': tables}


def summarize_legacy(root, expected_hosts):
    if not expected_hosts or len(set(expected_hosts)) != len(expected_hosts):
        raise ValueError('legacy summaries require explicit unique expected hosts')
    if any(root.rglob('cache_snapshot.json')):
        raise ValueError('cannot mix legacy and manifest inputs')
    tables = {name: {'used_bytes': 0, 'entry_count': 0, 'files_found': 0} for name in TABLE_FILES}
    found = set()
    host_prefixes = {}
    for path in root.rglob('*.output'):
        for name, suffix in TABLE_FILES.items():
            if path.name == suffix:
                raise ValueError('legacy file lacks host identity')
            if not path.name.endswith('-' + suffix):
                continue
            prefix = path.name[:-len(suffix)-1]
            matches = [host for host in expected_hosts if prefix == host or (prefix.startswith(host + '-') and prefix[len(host)+1:].isdigit())]
            if len(matches) != 1:
                raise ValueError('ambiguous or unexpected legacy host')
            host = matches[0]
            if host_prefixes.setdefault(host, prefix) != prefix:
                raise ValueError('mixed legacy process identities for host')
            if host not in expected_hosts or (host, name) in found:
                raise ValueError('unexpected host or duplicate legacy host/table')
            found.add((host, name))
            size, entries = parse_cache_size_file(path)
            tables[name]['used_bytes'] += size
            tables[name]['entry_count'] += entries
            tables[name]['files_found'] += 1
    missing = sorted(host for host in expected_hosts if any((host, name) not in found for name in TABLE_FILES))
    return {'ok': not missing, 'verification': 'legacy-unverified',
            'warning': 'Run, epoch, snapshot coherence and historical host coverage cannot be verified.',
            'coverage': {'expected': sorted(expected_hosts), 'missing': missing}, 'tables': tables}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metrics_root', type=Path)
    parser.add_argument('--expected-host', action='append', default=[])
    parser.add_argument('--run-id')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--legacy', action='store_true')
    parser.add_argument('--csv')
    args = parser.parse_args()
    try:
        result = summarize_legacy(args.metrics_root, args.expected_host) if args.legacy else summarize_root(args.metrics_root, args.expected_host, args.run_id)
        if args.csv:
            with open(args.csv, 'w', newline='') as stream:
                writer = csv.writer(stream)
                writer.writerow(['table', 'used_bytes', 'entry_count', 'files_found', 'verification', 'expected_hosts', 'missing_hosts'])
                for name, item in result['tables'].items():
                    writer.writerow([name, item['used_bytes'], item['entry_count'], item['files_found'], result['verification'],
                                     ';'.join(result['coverage']['expected']), ';'.join(result['coverage']['missing'])])
    except (OSError, ValueError, KeyError, TypeError) as exc:
        result = {'ok': False, 'error': str(exc)}
    print(json.dumps(result, indent=None if args.json else 2))
    return 0 if result['ok'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
