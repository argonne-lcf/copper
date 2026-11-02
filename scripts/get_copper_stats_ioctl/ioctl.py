#!/usr/bin/env python3
"""Named Copper IOCTL operations; no implicit creation of control files."""
import argparse
import fcntl
import json
import os

COMMANDS = {
    'data-cache-size': (2300, False), 'tree-cache-size': (2400, False),
    'metadata-cache-size': (2500, False), 'all-metrics': (2000, False),
    'cache-tables': (200, False), 'operation-count': (300, False),
    'operation-time': (400, False), 'cache-hits': (500, False), 'cache-misses': (600, False),
    'reset-fs': (1900, True), 'clear-cache': (100, True),
}

class JsonParser(argparse.ArgumentParser):
    def error(self, message):
        print(json.dumps({'ok': False, 'error': message}))
        raise SystemExit(2)


def main():
    parser = JsonParser(description=__doc__)
    parser.add_argument('command', choices=COMMANDS)
    parser.add_argument('--device', default=os.environ.get('SPEC_DEV_VIEW'))
    parser.add_argument('--allow-mutation', action='store_true')
    args = parser.parse_args()
    request, mutating = COMMANDS[args.command]
    receipt = {'ok': False, 'command': args.command, 'request': request, 'mutating': mutating}
    try:
        if mutating and not args.allow_mutation:
            raise ValueError('mutating command requires --allow-mutation')
        if not args.device:
            raise ValueError('--device or SPEC_DEV_VIEW is required')
        fd = os.open(args.device, (os.O_RDWR if mutating else os.O_RDONLY) | os.O_CLOEXEC | os.O_NOFOLLOW)
        try:
            receipt['result'] = fcntl.ioctl(fd, request, 0)
            receipt['ok'] = True
        finally:
            os.close(fd)
    except (OSError, ValueError) as exc:
        receipt.update(error=str(exc), errno=getattr(exc, 'errno', None))
    print(json.dumps(receipt))
    return 0 if receipt['ok'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
