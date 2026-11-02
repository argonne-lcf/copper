#!/usr/bin/env python3
"""Fail-closed job-scoped Copper shutdown."""
import argparse
import json
import sys
from pathlib import Path


def exact_mount(path, text=None):
    """Read the exact mountpoint, never a containing filesystem."""
    import re
    text = Path('/proc/self/mountinfo').read_text() if text is None else text
    found = []
    for line in text.splitlines():
        left, right = line.split(' - ', 1)
        fields, tail = left.split(), right.split()
        unescape = lambda value: re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), value)
        if unescape(fields[4]) == path:
            found.append({'mount_id': int(fields[0]), 'fstype': tail[0], 'source': unescape(tail[1])})
    if len(found) > 1:
        raise ValueError('stacked mounts are ambiguous')
    return found[0] if found else None


def load_manifest(path, job_id):
    import os, stat
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd) as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o022:
            raise ValueError('manifest must be a regular file owned by caller, not group/world writable')
        data = json.load(stream)
    hosts = data['expected_hosts']
    if data['schema_version'] != 1 or data['job_id'] != job_id or not job_id:
        raise ValueError('manifest schema or job mismatch')
    if not hosts or len(set(hosts)) != len(hosts) or set(hosts) != set(data['nodes']):
        raise ValueError('manifest host coverage mismatch')
    return data


def verify_identity(record):
    import os, pwd
    pid = record['pid']
    if type(pid) is not int or pid <= 1 or pid == os.getpid():
        raise ValueError('unsafe pid')
    base = Path('/proc') / str(pid)
    if record['uid'] != os.getuid() or record['user'] != pwd.getpwuid(os.getuid()).pw_name:
        raise ValueError('user identity mismatch')
    if record['boot_id'] != Path('/proc/sys/kernel/random/boot_id').read_text().strip():
        raise ValueError('boot identity mismatch')
    if base.stat().st_uid != record['uid']:
        raise ValueError('process owner mismatch')
    fields = (base / 'stat').read_text().rsplit(')', 1)[1].split()
    if int(fields[19]) != record['starttime']:
        raise ValueError('process starttime mismatch')
    if os.readlink(base / 'exe') != record['exe']:
        raise ValueError('executable mismatch')
    mount = record['mount']
    if not os.path.isabs(mount) or os.path.normpath(mount) != mount or mount == '/':
        raise ValueError('mount must be an absolute normalized non-root path')
    argv = (base / 'cmdline').read_bytes().split(b'\0')
    expected = [b'-vpath', os.fsencode(mount)]
    if not any(argv[i:i+2] == expected for i in range(len(argv)-1)):
        raise ValueError('process mount argument mismatch')
    mounted = exact_mount(mount)
    if mounted is not None and mounted != record['mount_identity']:
        raise ValueError('mount identity mismatch')
    if mounted is not None and not mounted['fstype'].startswith('fuse'):
        raise ValueError('refusing non-FUSE mount')
    return mounted


def stop_local(record, timeout):
    import os, select, signal, subprocess
    verify_identity(record)
    # Bind before the final verification; signals cannot hit a recycled PID.
    import ctypes
    libc = ctypes.CDLL(None, use_errno=True)
    fd = libc.pidfd_open(record['pid'], 0)
    if fd < 0:
        raise OSError(ctypes.get_errno(), 'pidfd_open failed')
    signals = []
    try:
        verify_identity(record)
        poller = select.poll(); poller.register(fd, select.POLLIN)
        for sig in (signal.SIGTERM, signal.SIGKILL):
            if poller.poll(0):
                break
            verify_identity(record)
            if libc.pidfd_send_signal(fd, int(sig), None, 0) < 0:
                raise OSError(ctypes.get_errno(), 'pidfd_send_signal failed')
            signals.append(sig.name)
            if poller.poll(int(timeout * 1000)):
                break
        exited = bool(poller.poll(0))
        mounted = exact_mount(record['mount'])
        unmount_error = None
        if exited and mounted is not None:
            if mounted != record['mount_identity']:
                raise ValueError('mount changed before unmount')
            try:
                result = subprocess.run(['fusermount3', '-u', record['mount']],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        universal_newlines=True, timeout=timeout)
                if result.returncode:
                    unmount_error = result.stderr.strip() or 'fusermount3 failed'
            except (OSError, subprocess.TimeoutExpired) as exc:
                unmount_error = str(exc)
        active = exact_mount(record['mount']) is not None
        return {'ok': exited and not active and unmount_error is None, 'process_exited': exited,
                'mount_active': active, 'signals': signals, 'unmount_error': unmount_error,
                'directory_removed': False}
    finally:
        os.close(fd)


def stop_cluster(data, args):
    import re, subprocess, shlex
    nodes = {}
    for host in data['expected_hosts']:
        receipt = {'host': host, 'job_id': args.job_id, 'ok': False}
        try:
            if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', host):
                raise ValueError('invalid host')
            remote = ['python3', str(Path(__file__).resolve()), '--manifest', str(Path(args.manifest).absolute()),
                      '--job-id', args.job_id, '--local', '--timeout', str(args.timeout)]
            if args.scheduler == 'aurora':
                remote_command = ' '.join(shlex.quote(part) for part in remote)
                command = ['clush', '-w', host, '-N', remote_command]
            elif args.scheduler == 'frontier':
                command = ['srun', '--overlap', '--kill-on-bad-exit=0', '-N', '1', '-n', '1',
                           '--ntasks-per-node=1', '--cpu-bind=none', '--nodelist', host, *remote]
            else:
                raise ValueError('--scheduler is required for distributed shutdown')
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    universal_newlines=True, timeout=3 * args.timeout + 10)
            received = json.loads(result.stdout)
            if received.get('host') != host or received.get('job_id') != args.job_id:
                raise ValueError('receipt identity mismatch')
            receipt.update(received)
            if result.returncode or received.get('ok') is not True:
                receipt['ok'] = False
                receipt['transport_error'] = result.stderr.strip() or 'remote failure'
        except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
            receipt['error'] = str(exc)
        nodes[host] = receipt
    return {'job_id': args.job_id, 'ok': all(r['ok'] for r in nodes.values()), 'nodes': nodes}


def main():
    import socket, math
    parser = argparse.ArgumentParser()
    parser.add_argument('--scheduler', choices=['aurora', 'frontier'])
    parser.add_argument('--manifest')
    parser.add_argument('--job-id')
    parser.add_argument('-d', '--log-dir')
    parser.add_argument('-v', '--mount')
    parser.add_argument('--local', action='store_true')
    parser.add_argument('--timeout', type=float, default=10)
    args = parser.parse_args()
    import os
    args.job_id = args.job_id or os.environ.get('PBS_JOBID' if args.scheduler == 'aurora' else 'SLURM_JOB_ID')
    if not args.job_id:
        parser.error('explicit --job-id or scheduler job identity is required')
    if not args.manifest:
        component = args.job_id.split('.', 1)[0] if args.scheduler == 'aurora' else args.job_id
        if Path(component).name != component or component in ('.', '..'):
            parser.error('unsafe job directory component')
        args.manifest = str(Path(args.log_dir or '~/copper-logs').expanduser() / component / 'daemon-identity.json')
    receipt = {'host': socket.gethostname(), 'job_id': args.job_id, 'ok': False}
    try:
        if not math.isfinite(args.timeout) or args.timeout <= 0:
            raise ValueError('timeout must be finite and positive')
        data = load_manifest(args.manifest, args.job_id)
        if args.mount and any(record.get('mount') != args.mount for record in data['nodes'].values()):
            raise ValueError('requested mount differs from daemon manifest')
        if not args.local:
            receipt = stop_cluster(data, args)
            print(json.dumps(receipt))
            return 0 if receipt['ok'] else 1
        receipt.update(stop_local(data['nodes'][socket.gethostname()], args.timeout))
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as exc:
        receipt['error'] = 'manifest/identity: ' + str(exc)
    print(json.dumps(receipt), file=sys.stdout if args.local else sys.stderr)
    return 0 if receipt['ok'] else 1

if __name__ == '__main__':
    sys.exit(main())
