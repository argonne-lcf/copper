#!/usr/bin/env python3
"""Bound both open and ioctl in a separate process (including blocked FIFO opens)."""
import json
import math
import os
from pathlib import Path
import subprocess
import sys


def main():
    try:
        timeout = float(os.environ.get('COPPER_IOCTL_TIMEOUT', '10'))
        if not math.isfinite(timeout) or timeout <= 0:
            raise ValueError('COPPER_IOCTL_TIMEOUT must be finite and positive')
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('ioctl.py')), *sys.argv[1:]],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                universal_newlines=True, timeout=timeout)
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        return result.returncode
    except subprocess.TimeoutExpired:
        print(json.dumps({'ok': False, 'timed_out': True, 'error': 'IOCTL timeout; completion is unknown, do not automatically retry mutations'}))
        return 124
    except (OSError, ValueError) as exc:
        print(json.dumps({'ok': False, 'error': str(exc)}))
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
