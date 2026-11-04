#!/usr/bin/env python3
"""Native IRCd reload/shutdown while Socket.connect is blocked in DNS.
Uses the socket cancellation test interposer, private /tmp BBS control data,
and loopback peers. No production IRC connections are made.
"""
import argparse
import os
from pathlib import Path
import select
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', required=True, type=Path)
parser.add_argument('--ctrl', required=True, type=Path)
args = parser.parse_args()
assert args.ctrl.resolve().is_relative_to(Path('/tmp')), 'Use private control data under /tmp'
assert sys.platform.startswith(('freebsd', 'linux')), 'Resolver timing fixture requires POSIX interposition'
source = Path(__file__).resolve().parents[2]
run = Path(tempfile.mkdtemp(prefix='ircd-connect-cancel-'))
shim = run / 'fixture.so'
subprocess.run(['cc', '-shared', '-fPIC', '-o', str(shim), str(source / 'tests/socket/connect-cancel-fixture.c')], check=True)
print('IRCd connection cancellation files:', run, flush=True)


def unused_port():
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        return reserve.getsockname()[1]


class Peer:
    def __init__(self, name, password):
        self.name, self.password = name, password
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen()
        self.listener.settimeout(12)
        self.port = self.listener.getsockname()[1]
        self.data = b''
        self.linked, self.done, self.stop = threading.Event(), threading.Event(), threading.Event()
        self.error = None
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def serve(self):
        try:
            with self.listener.accept()[0] as connection:
                connection.settimeout(.1)
                replied = False
                while not self.stop.is_set():
                    try:
                        data = connection.recv(16384)
                    except socket.timeout:
                        continue
                    if not data:
                        break
                    self.data += data
                    if not replied and b'\r\nSERVER ' in self.data:
                        connection.sendall(f'PASS {self.password} :TS\r\nCAPAB TS3 TSMODE TSJOIN\r\nSERVER {self.name} 1 :Private peer\r\n'.encode())
                        replied = True
                        self.linked.set()
        except OSError as error:
            self.error = error
        finally:
            self.listener.close()
            self.done.set()

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)


def receive(stream, marker, timeout=6):
    data = b''
    deadline = time.monotonic() + timeout
    while marker not in data and time.monotonic() < deadline:
        if select.select([stream], [], [], .05)[0]:
            part = stream.recv(16384)
            if not part:
                break
            data += part
    assert marker in data, repr(marker) + ' missing: ' + repr(data)
    return data


def client(port, nick):
    stream = socket.create_connection(('127.0.0.1', port), timeout=2)
    stream.sendall(f'NICK {nick}\r\nUSER {nick} 0 * :Cancellation test\r\n'.encode())
    receive(stream, b' 376 ')
    return stream


def config_text(port, peer, delayed):
    return f'''[Info]
Servername=cancel.ircd.test
[Port:{port}]
Default=true
[Class:1]
PingFrequency=120
ConnectFrequency=0
Maximum=100
SendQ=2000000
[Allow]
Mask=*@*
Class=1
[Operator:testop]
Nick=testop
Mask=*@*
Password=testpass
Flags=OD
Class=1
[Services:empty]
Disabled=true
[Server:target]
Servername={peer.name}
Hostname={'socket-delay.invalid' if delayed else '127.0.0.1'}
Port={peer.port}
Password={peer.password}
Class=1
Hub=true
'''


for mode in ['failed-rehash', 'successful-rehash', 'shutdown']:
    work = run / mode
    work.mkdir()
    old = Peer('old.test', 'obsolete-only')
    fresh = Peer('fresh.test', 'replacement-only') if mode == 'successful-rehash' else None
    port = unused_port()
    config = work / 'ircd.ini'
    config.write_text(config_text(port, old, True))
    env = os.environ.copy()
    env.update(LD_PRELOAD=str(shim), CONNECT_TEST_READY=str(work / 'ready'), CONNECT_TEST_RELEASE=str(work / 'release'))
    process = None
    controls = []
    try:
        with (work / 'native.log').open('w') as output:
            process = subprocess.Popen([str(args.jsexec), '-n', '-c', str(args.ctrl), '-i', str(source / 'load'),
                                        str(source / 'ircd.js'), '-a', '127.0.0.1', '-f', str(config)],
                                       env=env, stdout=output, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        deadline = time.monotonic() + 12
        while True:
            try:
                operator = client(port, 'testop')
                break
            except OSError:
                assert process.poll() is None and time.monotonic() < deadline, (work / 'native.log').read_text()
                time.sleep(.05)
        controls.append(operator)
        operator.sendall(b'OPER testop testpass\r\n')
        receive(operator, b' 381 ')
        observer = client(port, 'observer')
        controls.append(observer)
        operator.sendall(f'CONNECT old.test {old.port}\r\n'.encode())
        deadline = time.monotonic() + 5
        while not (work / 'ready').exists():
            assert time.monotonic() < deadline, 'Old connection did not reach DNS'
            time.sleep(.01)
        observer.sendall(b'PING :dns-responsive\r\n')
        receive(observer, b'dns-responsive', timeout=2)
        time.sleep(2.1)  # Reset the operator's command flood window.
        if mode == 'failed-rehash':
            hidden = work / 'hidden.ini'
            config.rename(hidden)
            try:
                operator.sendall(b'REHASH\r\n')
                receive(operator, b'Configuration reload failed')
            finally:
                hidden.rename(config)
            (work / 'release').write_text('release')
            assert old.linked.wait(6), 'Failed reload cancelled the original connection'
            assert b'PASS obsolete-only :TS\r\n' in old.data
        elif mode == 'successful-rehash':
            config.write_text(config_text(port, fresh, False))
            operator.sendall(b'REHASH\r\n')
            receive(operator, b' 382 ')
            operator.sendall(f'CONNECT fresh.test {fresh.port}\r\n'.encode())
            assert fresh.linked.wait(6), 'Replacement connection slot was not released'
            assert b'PASS replacement-only :TS\r\n' in fresh.data and b'obsolete-only' not in fresh.data
            observer.sendall(b'PING :replacement-responsive\r\n')
            receive(observer, b'replacement-responsive', timeout=2)
            (work / 'release').write_text('release')
            assert old.done.wait(6) and not old.data, 'Cancelled old attempt sent IRC state: ' + repr(old.data)
            assert not fresh.done.is_set(), 'Old completion disturbed the replacement connection'
        else:
            operator.sendall(b'DIE\r\n')
            time.sleep(.1)
            assert process.poll() is None, 'Teardown discarded pending native cleanup'
            (work / 'release').write_text('release')
            assert old.done.wait(6) and not old.data, 'Shutdown sent stale credentials: ' + repr(old.data)
            assert process.wait(timeout=6) == 0, (work / 'native.log').read_text()
        if mode != 'shutdown':
            observer.sendall(b'PING :after-dns\r\n')
            receive(observer, b'after-dns', timeout=2)
        print('PASS:', mode, flush=True)
    finally:
        (work / 'release').write_text('release')
        for stream in controls:
            stream.close()
        if process and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=8)
        old.close()
        if fresh:
            fresh.close()
print('All native IRCd DNS cancellation tests passed.', flush=True)
