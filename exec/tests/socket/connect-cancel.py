#!/usr/bin/env python3
"""Deterministic POSIX native Socket.connect cancellation tests.
Only private /tmp control data and loopback sockets are used. The interposer
changes DNS timing and injects startup errors; it never closes application sockets.
"""
import argparse
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', required=True, type=Path)
parser.add_argument('--ctrl', required=True, type=Path)
args = parser.parse_args()
if not sys.platform.startswith(('freebsd', 'linux')):
    raise SystemExit('This resolver interposer requires FreeBSD or Linux; run connect-callbacks.js for portable coverage.')
assert args.ctrl.resolve().is_relative_to(Path('/tmp')), 'Use a private /tmp BBS fixture'
source = Path(__file__).resolve().parent
run = Path(tempfile.mkdtemp(prefix='socket-connect-cancel-'))
print('Connection lifecycle test files:', run, flush=True)
shim = run / 'fixture.so'
subprocess.run(['cc', '-shared', '-fPIC', '-o', str(shim), str(source / 'connect-cancel-fixture.c')], check=True)
results = []
for mode in ['cancel-dns', 'cancel-dns-fail', 'close-ready', 'teardown-closed', 'teardown-open', 'teardown-ready', 'exception', 'pair-failure', 'thread-failure']:
    work = run / mode
    work.mkdir()
    env = os.environ.copy()
    env['LD_PRELOAD'] = str(shim)
    for key, filename in [('READY', 'ready'), ('RELEASE', 'release'), ('NOTIFIED', 'notified'), ('TRACKED', 'tracked'), ('CLOSED', 'closed'), ('ARM', 'arm')]:
        env['CONNECT_TEST_' + key] = str(work / filename)
    if mode == 'cancel-dns-fail':
        env['CONNECT_TEST_DNS_FAIL'] = '1'
    if mode in ('pair-failure', 'thread-failure'):
        env['CONNECT_TEST_FAULT'] = mode.split('-')[0]
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen()
        listener.setblocking(False)
        command = [str(args.jsexec), '-n', '-c', str(args.ctrl), str(source / 'connect-cancel.sjs'), mode,
                   str(listener.getsockname()[1]), str(work)]
        process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        stream = None
        data = b''
        try:
            if not mode.endswith('-failure'):
                deadline = time.monotonic() + 8
                while not (work / 'script-ready').exists():
                    assert process.poll() is None, process.communicate()[0]
                    assert time.monotonic() < deadline, 'JS fixture did not become ready'
                    time.sleep(.01)
                if mode not in ('close-ready', 'teardown-ready'):
                    assert not (work / 'closed').exists(), 'Application descriptor closed before DNS finished'
                    time.sleep(.05)
                    assert process.poll() is None, 'Pending native cleanup was discarded during teardown'
                    (work / 'release').write_text('release')
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if stream is None and select.select([listener], [], [], .02)[0]:
                    stream = listener.accept()[0]
                    stream.setblocking(False)
                if stream is not None and select.select([stream], [], [], .02)[0]:
                    part = stream.recv(4096)
                    if not part:
                        break
                    data += part
                if process.poll() is not None and stream is None:
                    break
            output, _ = process.communicate(timeout=3)
            (work / 'native.log').write_text(output)
            expected = 7 if mode == 'exception' else 0
            assert process.returncode == expected, output
            assert ('EXPECTED_CONNECT_TEARDOWN' in output) if mode == 'exception' else ('PASS: ' + mode in output or 'RETURNING: ' + mode in output), output
            assert data == (b'RECOVERED\r\n' if mode.endswith('-failure') else b''), repr(data)
            if not mode.endswith('-failure'):
                assert (work / 'closed').read_text() == 'main', 'Native socket was not closed by the main thread'
            results.append({'mode': mode, 'exit': process.returncode})
            print('PASS:', mode, flush=True)
        finally:
            if stream is not None:
                stream.close()
            if process.poll() is None:
                (work / 'release').write_text('release')
                process.kill()
                process.wait(timeout=5)
(run / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('All delayed-connect lifecycle tests passed.', flush=True)
