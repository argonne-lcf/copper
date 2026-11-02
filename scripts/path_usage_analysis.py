#!/usr/bin/env python3
"""Compare historical observations to explicit inventories, never infer deletability."""
import argparse
import json
import os
from pathlib import Path
import sqlite3
import tempfile


def under_roots(path, roots):
    return not roots or any(path == root or path.startswith(root.rstrip('/') + '/') for root in roots)


def observed_paths(log_dir, roots):
    tables = log_dir/'tables/final'
    if not tables.is_dir():
        tables = log_dir
    for file in tables.glob('*.output'):
        with file.open() as stream:
            for line in stream:
                if line.startswith('/'):
                    path = line.rstrip('\n').split(' {', 1)[0]
                    if under_roots(path, roots):
                        yield path


def load_inventory(path, db, roots):
    with path.open() as stream:
        metadata = json.loads(next(stream))
        if metadata['schema_version'] != 1 or metadata['kind'] != 'path-inventory':
            raise ValueError('unsupported inventory schema')
        if not isinstance(metadata['captured_at'], str) or not metadata['captured_at'] or type(metadata['complete']) is not bool:
            raise ValueError('invalid inventory capture metadata')
        if not isinstance(metadata['roots'], list) or not metadata['roots'] or any(not os.path.isabs(p) for p in metadata['roots']):
            raise ValueError('invalid inventory roots')
        count = 0
        for line in stream:
            row = json.loads(line)
            value = row['path']
            if not os.path.isabs(value) or os.path.normpath(value) != value or not under_roots(value, metadata['roots']):
                raise ValueError('invalid or out-of-scope inventory path')
            if row['type'] not in ('file', 'directory', 'symlink', 'other'):
                raise ValueError('invalid inventory entry type')
            count += 1
            if under_roots(value, roots):
                db.execute('INSERT INTO inventory VALUES (?,?)', (value, row['type']))
        if 'entry_count' in metadata and (type(metadata['entry_count']) is not int or metadata['entry_count'] != count):
            raise ValueError('inventory entry count mismatch')
    return metadata


def walk_inventory(args, db):
    import datetime, math, stat, time
    if not args.usage_root or not args.save_inventory:
        raise ValueError('--walk requires explicit --usage-root and --save-inventory')
    if args.max_entries <= 0 or args.max_depth < 0 or not math.isfinite(args.max_seconds) or args.max_seconds <= 0:
        raise ValueError('walk bounds must be positive (depth may be zero)')
    roots = sorted(set(os.path.normpath(p) for p in args.usage_root))
    if any(not os.path.isabs(p) or p == '/' for p in roots):
        raise ValueError('walk roots must be absolute and not filesystem root')
    roots = [p for p in roots if not any(p != q and p.startswith(q + '/') for q in roots)]
    db.execute('CREATE TABLE walk_queue(path TEXT PRIMARY KEY, depth INTEGER) WITHOUT ROWID')
    db.executemany('INSERT INTO walk_queue VALUES (?,0)', ((p,) for p in roots))
    deadline = time.monotonic() + args.max_seconds
    count = 0
    complete = True
    errors = []
    while True:
        pending = db.execute('SELECT path, depth FROM walk_queue LIMIT 1').fetchone()
        if pending is None:
            break
        path, depth = pending
        db.execute('DELETE FROM walk_queue WHERE path=?', (path,))
        if time.monotonic() >= deadline or count >= args.max_entries:
            complete = False
            break
        try:
            mode = os.lstat(path).st_mode
            kind = 'symlink' if stat.S_ISLNK(mode) else 'directory' if stat.S_ISDIR(mode) else 'file' if stat.S_ISREG(mode) else 'other'
            cursor = db.execute('INSERT OR IGNORE INTO inventory VALUES (?,?)', (path, kind))
            count += cursor.rowcount
            if kind == 'directory':
                if depth >= args.max_depth:
                    complete = False
                    continue
                # scandir streams entries; SQLite stores the traversal queue on disk in bounded mode.
                with os.scandir(path) as entries:
                    for entry in entries:
                        if time.monotonic() >= deadline:
                            complete = False
                            break
                        queued = db.execute('SELECT count(*) FROM walk_queue').fetchone()[0]
                        if count + queued >= args.max_entries:
                            complete = False
                            break
                        db.execute('INSERT OR IGNORE INTO walk_queue VALUES (?,?)', (entry.path, depth + 1))
        except OSError as exc:
            complete = False
            if len(errors) < 20:
                errors.append(str(exc))
    metadata = {'schema_version': 1, 'kind': 'path-inventory', 'captured_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'roots': roots, 'complete': complete, 'entry_count': count, 'errors': errors,
                'temporal_semantics': 'live non-atomic traversal, not the historical profiled run'}
    args.save_inventory.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=args.save_inventory.parent, prefix='.inventory-')
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(json.dumps(metadata)+'\n')
            for path, kind in db.execute('SELECT path, type FROM inventory ORDER BY path'):
                stream.write(json.dumps({'path': path, 'type': kind})+'\n')
            stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, args.save_inventory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return metadata


def analyze(args, db):
    output = args.output_dir or args.log_dir/'paths_dir'
    output.mkdir(parents=True, exist_ok=True)
    db.execute('PRAGMA cache_size=-2048')
    db.execute('PRAGMA temp_store=FILE')
    db.execute('CREATE TABLE observed(path TEXT PRIMARY KEY) WITHOUT ROWID')
    db.execute('CREATE TABLE inventory(path TEXT PRIMARY KEY, type TEXT) WITHOUT ROWID')
    db.executemany('INSERT OR IGNORE INTO observed VALUES (?)', ((p,) for p in observed_paths(args.log_dir, args.usage_root)))
    if args.inventory and args.walk:
        raise ValueError('--inventory and --walk are mutually exclusive')
    metadata = walk_inventory(args, db) if args.walk else load_inventory(args.inventory, db, args.usage_root) if args.inventory else None
    result = {'ok': metadata is None or metadata['complete'] is True, 'inventory_source': 'live' if args.walk else 'saved' if metadata else 'none',
              'historical_existence': 'unknown', 'live_walk_performed': args.walk,
              'observation_verification': 'legacy-unverified', 'inventory_metadata': metadata}
    queries = {
        'observed_count': ('observed_paths.txt', 'SELECT path FROM observed'),
        'observed_in_inventory': ('observed_in_inventory.txt', 'SELECT path FROM observed INTERSECT SELECT path FROM inventory'),
        'observed_not_in_inventory': ('observed_not_in_inventory.txt', 'SELECT path FROM observed EXCEPT SELECT path FROM inventory'),
        'inventory_not_observed': ('inventory_not_observed.txt', 'SELECT path FROM inventory EXCEPT SELECT path FROM observed'),
    }
    for key, (filename, query) in queries.items():
        count = 0
        with (output/filename).open('w') as stream:
            for (path,) in db.execute(query + ' ORDER BY path'):
                stream.write(path+'\n'); count += 1
        result[key] = count
    (output/'paths_summary.md').write_text(
        '# Path observation analysis\n\nHistorical existence during the profiled run is unknown.\n'
        + ('Comparison uses an inventory at its recorded capture time, not the historical run.\n' if metadata else 'No inventory or live walk was requested.\n')
        + 'Absence from an inventory is not proof of historical nonexistence.\n'
        + 'Non-observation is not evidence of dispensability. No deletion recommendations are made.\n'
        + '\n```json\n' + json.dumps(result, indent=2) + '\n```\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log_dir', type=Path)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--usage-root', action='append', default=[])
    parser.add_argument('--inventory', type=Path)
    parser.add_argument('--walk', action='store_true')
    parser.add_argument('--save-inventory', type=Path)
    parser.add_argument('--max-entries', type=int, default=100000)
    parser.add_argument('--max-depth', type=int, default=32)
    parser.add_argument('--max-seconds', type=float, default=30)
    parser.add_argument('--memory-bounded', action='store_true', help='disk-backed SQLite indexes with bounded page cache')
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix='copper-paths-') as temporary:
            db = sqlite3.connect(str(Path(temporary)/'paths.sqlite') if args.memory_bounded else ':memory:')
            try:
                result = analyze(args, db)
            finally:
                db.close()
    except (OSError, ValueError, KeyError, TypeError, StopIteration, sqlite3.Error) as exc:
        result = {'ok': False, 'error': str(exc)}
    print(json.dumps(result))
    return 0 if result['ok'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
