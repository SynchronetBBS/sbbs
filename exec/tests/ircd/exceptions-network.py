#!/usr/bin/env python3
"""Native three-server channel exceptions, ChanServ persistence, and an actual
legacy leaf. Requires services-fixture.py and a saved older exec tree.
Only private /tmp BBS fixtures are used; every child exit is checked.
"""
import argparse
from pathlib import Path
import select
import signal
import socket
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', type=Path, required=True)
parser.add_argument('--legacy-source', type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
fixture = Path('/tmp/ircd-services-bbs-fixture')
run = Path(tempfile.mkdtemp(prefix='ircd-exceptions-network-'))
nodes, clients = {}, []
print('Exception test files:', run, flush=True)


def port():
    with socket.socket() as stream:
        stream.bind(('127.0.0.1', 0))
        return stream.getsockname()[1]


for name in ['hub', 'home', 'edge', 'old']:
    nodes[name] = {'port': port(), 'process': None, 'log': run / (name + '.log')}


def start(name):
    node = nodes[name]
    code = args.legacy_source if name == 'old' else source
    peers = {'hub': [('home', False, True), ('old', False, False)],
             'home': [('hub', True, True), ('edge', False, False)],
             'edge': [('home', True, True)], 'old': [('hub', True, True)]}[name]
    lines = ['[Info]', 'Servername=' + name + '.test', 'Description=Exception regression',
             '[Port:' + str(node['port']) + ']', 'Default=true',
             '[Class:1]', 'PingFrequency=120', 'ConnectFrequency=0', 'Maximum=100', 'SendQ=2000000',
             '[Class:30]', 'PingFrequency=120', 'ConnectFrequency=1', 'Maximum=20', 'SendQ=15000000',
             '[Allow]', 'Mask=*@*', 'Class=1', '[Services:embedded]', 'Servername=127.0.0.1']
    for other, outbound, hub in peers:
        lines += ['[Server:' + other + ']', 'Servername=' + other + '.test', 'Hostname=127.0.0.1',
                  'Password=exceptiontest', 'Class=30', 'Hub=' + str(hub).lower(),
                  'Port=' + str(nodes[other]['port'] if outbound else 0)]
    if name != 'old':
        lines += ['[BBSServices]', 'NickServ=true', '[EmbeddedServices]', 'Host=127.0.0.1',
                  'Database=' + str(run / (name + '.sqlite'))]
        if name == 'hub':
            lines += ['Enabled=true']
    legacy_conf = name == 'old' and (code / 'load/ircd_server.js').exists()
    if legacy_conf:
        lines = ['M:old.test:127.0.0.1:Old leaf:' + str(node['port']),
                 'A:Test:localhost:test@localhost', 'Y:1:120:0:100:2000000',
                 'Y:30:120:1:20:15000000', 'I:*::*::1', 'U:127.0.0.1',
                 'C:127.0.0.1:exceptiontest:hub.test:' + str(nodes['hub']['port']) + ':30',
                 'N:127.0.0.1:exceptiontest:hub.test::30', 'H:*::hub.test']
    config = run / (name + ('.conf' if legacy_conf else '.ini'))
    config.write_text('\n'.join(lines) + '\n')
    ctrl = fixture / (name if name != 'old' else '') / 'ctrl'
    with node['log'].open('a') as log:
        node['process'] = subprocess.Popen([str(args.jsexec), '-n', '-L7', '-c', str(ctrl),
                                           '-i', str(code / 'load'), str(code / 'ircd.js'),
                                           '-a', '127.0.0.1', '-f', str(config)],
                                          stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    node['running'] = True
    end = time.monotonic() + 30
    while True:
        assert node['process'].poll() is None, node['log'].read_text()
        try:
            with socket.create_connection(('127.0.0.1', node['port']), timeout=.5):
                return
        except OSError:
            assert time.monotonic() < end, node['log'].read_text()
            time.sleep(.1)


class Client:
    def __init__(self, node, nick, identify=False):
        self.node, self.nick = node, nick
        self.sock = socket.create_connection(('127.0.0.1', nodes[node]['port']), timeout=5)
        self.buffer = b''
        clients.append(self)
        if identify:
            self.send('PASS TESTPASS')
        self.send('NICK ' + nick)
        self.send('USER ' + ('Some.User' if identify else 'viewer') + ' 0 * :Exception test')
        self.expect(' 376 ')

    def send(self, text):
        for node in nodes.values():
            if node.get('running'):
                assert node['process'].poll() is None, node['log'].read_text()
        self.sock.sendall((text + '\r\n').encode())

    def read(self, timeout):
        end = time.monotonic() + timeout
        while True:
            if b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                line = line.rstrip(b'\r').decode('latin1')
                if line.startswith('PING '):
                    self.send('PONG ' + line[5:])
                return line
            left = end - time.monotonic()
            if left <= 0 or not select.select([self.sock], [], [], min(left, .2))[0]:
                if left <= 0:
                    return None
                continue
            data = self.sock.recv(16384)
            assert data, 'Client disconnected: ' + self.nick
            self.buffer += data

    def expect(self, text, timeout=15):
        end, seen = time.monotonic() + timeout, []
        while time.monotonic() < end:
            line = self.read(end - time.monotonic())
            if line is None:
                break
            seen.append(line)
            if text in line:
                return line
        raise AssertionError('Expected ' + text + '; saw:\n' + '\n'.join(seen))

    def mode(self, text):
        self.send('MODE #test ' + text)
        self.expect(' MODE #test ')

    def exceptions(self):
        self.send('MODE #test +e')
        result = []
        while True:
            line = self.read(15)
            assert line, 'No exception list completion'
            if ' 348 ' in line:
                result.append(line.split(' #test ', 1)[1].split(' ')[0])
            if ' 349 ' in line:
                return result

    def wait_exceptions(self, expected):
        end = time.monotonic() + 20
        while True:
            actual = self.exceptions()
            if {s.upper() for s in actual} == {s.upper() for s in expected}:
                return
            assert time.monotonic() < end, (expected, actual)
            time.sleep(.2)

    def service(self, text, expected):
        time.sleep(1.1)
        self.send('PRIVMSG ChanServ :' + text)
        return self.expect(expected)


try:
    for name in ['hub', 'home', 'edge']:
        start(name)
    owner = Client('hub', 'Owner', True)
    owner.send('JOIN #test')
    owner.expect(' 366 ')
    listener = Client('home', 'Listener')
    listener.send('JOIN #test')
    listener.expect(' 366 ')
    outsider = Client('edge', 'Outside')
    # Wait for the complete updated topology and services startup.
    end = time.monotonic() + 25
    while True:
        owner.send('WHOIS NickServ')
        line = owner.expect(' 318 ')
        owner.send('LINKS')
        links = []
        while True:
            line = owner.read(15)
            assert line
            links.append(line)
            if ' 365 ' in line:
                break
        if any(' edge.test ' in line for line in links):
            try:
                owner.service('REGISTER #test Test', 'Channel registered')
                break
            except AssertionError:
                pass
        assert time.monotonic() < end, 'Topology/services startup failed'
        time.sleep(.3)
    owner.mode('+mn')
    outsider.send('PRIVMSG #test :blocked outside')
    outsider.expect(' 404 ')
    ext, mod = '~msgbypass:external:Outside!*@*', '~msgbypass:moderated:Outside!*@*'
    owner.mode('+e ~m:external:Outside')
    outsider.wait_exceptions([ext])
    outsider.send('PRIVMSG #test :still moderated')
    outsider.expect(' 404 ')
    owner.mode('+e ~m:moderated:Outside')
    outsider.wait_exceptions([ext, mod])
    for verb in ['PRIVMSG', 'NOTICE']:
        outsider.send(verb + ' #test :external success')
        owner.expect(verb + ' #test :external success')
        listener.expect(verb + ' #test :external success')
    outsider.send('WHOIS Outside')
    line = outsider.expect(' 318 ')
    outsider.send('NAMES #test')
    assert 'Outside' not in outsider.expect(' 353 ').split(' :', 1)[1], 'Exception joined the sender'
    owner.send('PRIVMSG #test :no subscription')
    end = time.monotonic() + .8
    while time.monotonic() < end:
        line = outsider.read(.2)
        assert not line or 'no subscription' not in line, line
    print('PASS: both bypasses work over two updated hops without membership or subscription', flush=True)

    owner.mode('+b Outside!*@*')
    time.sleep(.4)
    outsider.send('PRIVMSG #test :ban remains')
    outsider.expect(' 404 ')
    owner.mode('+e Outside!*@*')
    outsider.wait_exceptions([ext, mod, 'Outside!*@*'])
    outsider.send('PRIVMSG #test :ban exempt')
    owner.expect('PRIVMSG #test :ban exempt')
    print('PASS: ban exceptions are independent of external and moderated bypasses', flush=True)

    owner.service('EXCEPT #test ADD ~m:external:Outside', 'Managed exception saved')
    owner.service('EXCEPT #test ADD ~m:moderated:Outside', 'Managed exception saved')
    owner.mode('-e ' + ext)
    outsider.wait_exceptions([ext, mod, 'Outside!*@*'])
    owner.service('EXCEPT #test ADD ~m:censor:Outside', 'Unsupported exception selectors')
    owner.service('EXCEPT #test LIST', 'End of managed exception list')
    owner.service('EXCEPT #test DEL ' + ext, 'Managed exception removed')
    outsider.wait_exceptions([mod, 'Outside!*@*'])
    owner.service('EXCEPT #test ADD ' + ext, 'Managed exception saved')
    # Reconnect the leaf to exercise actual burst and CSYNC replacement.
    outsider.sock.close()
    nodes['edge']['process'].send_signal(signal.SIGTERM)
    assert nodes['edge']['process'].wait(timeout=15) in (0, -signal.SIGTERM)
    time.sleep(1)
    start('edge')
    outsider = Client('edge', 'Outside')
    outsider.wait_exceptions([ext, mod, 'Outside!*@*'])
    outsider.send('PRIVMSG #test :after leaf reconnect')
    owner.expect('PRIVMSG #test :after leaf reconnect')
    print('PASS: ChanServ adopts, restores and deletes grants; leaf reconnect receives complete state', flush=True)

    owner.mode('+o Listener')
    listener.expect('+o Listener')
    owner.sock.close()
    nodes['hub']['process'].send_signal(signal.SIGTERM)
    assert nodes['hub']['process'].wait(timeout=15) in (0, -signal.SIGTERM)
    nodes['hub']['running'] = False
    listener.mode('-e ' + ext)
    outsider.wait_exceptions([mod, 'Outside!*@*'])
    start('hub')
    owner = Client('hub', 'Owner', True)
    owner.send('JOIN #test')
    owner.expect(' 366 ')
    outsider.wait_exceptions([ext, mod, 'Outside!*@*'])
    end = time.monotonic() + 20
    while True:
        owner.send('NAMES #test')
        names = owner.expect(' 353 ')
        if '@Owner' in names:
            break
        assert time.monotonic() < end, names
        time.sleep(.3)
    owner.service('EXCEPT #test LIST', 'End of managed exception list')
    outsider.send('PRIVMSG #test :after authority restart')
    owner.expect('PRIVMSG #test :after authority restart')
    print('PASS: authority restart reloads saved grants and restores a grant removed during its absence', flush=True)

    start('old')
    old = Client('old', 'Legacy')
    old.send('JOIN #test')
    old.expect(' 366 ')
    # Removing -l separately and filtering +e must keep +v's nickname intact.
    owner.mode('+l 50')
    owner.mode('-l+ev Extra!*@* Legacy')
    old.expect('+v Legacy')
    old.send('NAMES #test')
    assert '+Legacy' in old.expect(' 353 ')
    old.send('PRIVMSG #test :legacy normal traffic')
    owner.expect('PRIVMSG #test :legacy normal traffic')
    outsider.wait_exceptions([ext, mod, 'Outside!*@*', 'Extra!*@*'])
    outsider.send('PRIVMSG #test :legacy stricter path')
    owner.expect('PRIVMSG #test :legacy stricter path')
    # Legacy +n still rejects the outside sender, as documented.
    end = time.monotonic() + 1
    while time.monotonic() < end:
        line = old.read(.2)
        assert not line or 'legacy stricter path' not in line, line
    print('PASS: actual legacy leaf remains usable, mixed MODE arguments survive, and unknown exceptions do not weaken restrictions', flush=True)
finally:
    for client in clients:
        try:
            client.sock.close()
        except OSError:
            pass
    failures = []
    for name, node in nodes.items():
        process = node['process']
        if not process:
            continue
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
        try:
            status = process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            process.kill()
            status = process.wait(timeout=5)
            failures.append(name + ' did not stop')
        if status not in (0, -signal.SIGTERM):
            failures.append(name + ' crashed: ' + str(status))
        print(name + ': exit=' + str(status) + ', log=' + str(node['log']), flush=True)
    assert not failures, '; '.join(failures)
