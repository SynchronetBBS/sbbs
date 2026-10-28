#!/usr/bin/env python3
"""Exercise embedded services through an actual older IRCd relay.

Requires services-fixture.py. Uses only its private /tmp BBS databases.
Every server is a direct child; crashes are checked independently of IRC
assertions, including shutdown exit status. No shell or live service is used.
"""
import argparse
from pathlib import Path
import resource
import select
import signal
import socket
import subprocess
import tempfile
import time
from types import SimpleNamespace

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', required=True, type=Path)
parser.add_argument('--legacy-source', required=True, type=Path)
parser.add_argument('--single-login-source', type=Path, help='Run edge with the earlier single-login services implementation')
parser.add_argument('--skip-grace', action='store_true', help='Skip the 60-second nickname enforcement test')
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
fixture = Path('/tmp/ircd-services-bbs-fixture')
run = Path(tempfile.mkdtemp(prefix='ircd-services-network-'))
servers = []
clients = []

def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

class Server:
    def __init__(self, name, port, bind='127.0.0.1', legacy=False):
        self.name, self.port, self.bind, self.legacy = name, port, bind, legacy
        self.sname = name + '.test'
        self.ctrl = fixture / (name if name in ['hub', 'home', 'edge'] else '') / 'ctrl'
        self.process = None

    def start(self, peers, central_enabled=True):
        code = args.legacy_source if self.legacy else (args.single_login_source if self.name == 'edge' and args.single_login_source else source)
        lines = ['[Info]', 'Servername=' + self.sname, 'Description=Embedded services isolated test',
                 '[Port:' + str(self.port) + ']', 'Default=true',
                 '[Class:1]', 'PingFrequency=120', 'ConnectFrequency=0', 'Maximum=100', 'SendQ=2000000',
                 '[Class:30]', 'PingFrequency=120', 'ConnectFrequency=1', 'Maximum=20', 'SendQ=15000000',
                 '[Allow]', 'Mask=*@*', 'Class=1',
                 '[Services:embedded]', 'Servername=127.0.0.1']
        for other, connect, hub in peers:
            lines += ['[Server:' + other.sname + ']', 'Servername=' + other.sname,
                      'Hostname=' + other.bind, 'Port=' + str(other.port if connect else 0),
                      'InboundPassword=servicetest', 'OutboundPassword=servicetest',
                      'Hub=' + str(hub).lower(), 'Class=30']
        if not self.legacy:
            lines += ['[BBSServices]', 'NickServ=' + str(self.name != 'hub').lower(),
                      'MemoServ=' + str(self.name != 'hub').lower(),
                      '[EmbeddedServices]', 'Host=127.0.0.1',
                      'Database=' + str(run / (self.name + '.sqlite'))]
            if self.name == 'hub':
                if central_enabled:
                    lines.append('Enabled=true')
                lines += ['[Server:127.0.0.1]', 'Servername=127.0.0.1', 'Hostname=127.0.0.1',
                          'Password=servicetest', 'Class=30',
                          '[Operator:testop]', 'Nick=testop', 'Mask=*@*',
                          'Password=servicetest', 'Flags=Or', 'Class=1']
        old_config = self.legacy and (code / 'load/ircd_server.js').exists() and not (code / 'load/ircd/server.js').exists()
        if old_config:
            lines = ['M:' + self.sname + ':127.0.0.1:Services test:' + str(self.port),
                     'A:Test Admin:localhost:test@' + self.sname,
                     'Y:1:120:0:100:2000000', 'Y:30:120:1:20:15000000', 'I:*::*::1', 'U:127.0.0.1']
            for other, connect, hub in peers:
                # The colon-delimited 1.3 configuration cannot contain IPv6.
                # These leaves connect outward to the relay's IPv4 socket.
                address = other.bind if connect else '127.0.0.1'
                lines += ['C:' + address + ':servicetest:' + other.sname + ':' + str(other.port if connect else 0) + ':30',
                          'N:' + address + ':servicetest:' + other.sname + '::30']
                if hub:
                    lines.append('H:*::' + other.sname)
        config = run / (self.name + ('.conf' if old_config else '.ini'))
        config.write_text('\n'.join(lines) + '\n')
        self.config_path = config
        self.log_path = run / (self.name + '.log')
        self.log = self.log_path.open('a')
        if self.name in ['home', 'edge'] and not getattr(self, 'mailboxes_reset', False):
            reset = ('if (system.data_dir.indexOf("/tmp/ircd-services-bbs-fixture/") !== 0) '
                     'throw new Error("Unsafe fixture directory"); '
                     'var n = system.matchuser("Some User", false); if (n) system.get_telegram(n);')
            subprocess.run([str(args.jsexec), '-n', '-c', str(self.ctrl), '-r', reset],
                           stdin=subprocess.DEVNULL, stdout=self.log, stderr=subprocess.STDOUT, preexec_fn=no_core, check=True)
            self.mailboxes_reset = True
        self.process = subprocess.Popen([str(args.jsexec), '-A', '-L7', '-c', str(self.ctrl), '-i', str(code / 'load'),
                                         str(code / 'ircd.js'), '-a', self.bind, '-f', str(config)],
                                        stdin=subprocess.DEVNULL, stdout=self.log, stderr=subprocess.STDOUT,
                                        preexec_fn=no_core)
        if self not in servers:
            servers.append(self)
        end = time.monotonic() + 20
        while time.monotonic() < end:
            self.check()
            try:
                test = socket.create_connection((self.bind, self.port), timeout=.2)
                test.close()
                return
            except OSError:
                time.sleep(.1)
        raise AssertionError('Server failed to listen: ' + self.name)

    def check(self):
        code = self.process.poll()
        if code is not None:
            raise AssertionError(f'{self.name} exited unexpectedly: {code}; {self.log_path}')

class Client:
    def __init__(self, server, nick, password=None, username='test'):
        self.server, self.nick = server, nick
        self.sock = socket.create_connection((server.bind, server.port), timeout=5)
        self.buffer = b''
        clients.append(self)
        if password:
            self.send('PASS ' + password)
        self.send('NICK ' + nick)
        self.send('USER ' + username + ' 0 * :Services test')
        self.expect(' 001 ')

    def send(self, text):
        for server in servers:
            server.check()
        self.sock.sendall((text + '\r\n').encode('latin1'))

    def expect(self, pattern, timeout=12):
        end, seen = time.monotonic() + timeout, []
        while time.monotonic() < end:
            for server in servers:
                server.check()
            if b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                line = line.rstrip(b'\r').decode('latin1')
                seen.append(line)
                if line.startswith('PING '):
                    self.send('PONG ' + line[5:])
                if pattern in line:
                    return line
                continue
            if select.select([self.sock], [], [], min(.2, max(0, end - time.monotonic())))[0]:
                data = self.sock.recv(16384)
                if not data:
                    raise AssertionError('Connection closed while waiting for ' + pattern + ': ' + '\n'.join(seen))
                self.buffer += data
        raise AssertionError('Expected ' + pattern + '; saw:\n' + '\n'.join(seen))

    def service(self, name, command, expected):
        time.sleep(1.05)
        self.send('PRIVMSG ' + name + ' :' + command)
        return self.expect(expected)

def group_memo(client, service, command, expected):
    line = client.service(service, command, expected)
    client.expect('Checked ')
    return line

hub = Server('hub', 19870)
relay = Server('relay', 19871, legacy=True)
home = Server('home', 19872, '::1')
edge = Server('edge', 19873, '::1')
try:
    hub.start([(relay, False, True)], central_enabled=False)
    observer = Client(hub, 'Watcher')
    time.sleep(6)
    observer.send('WHOIS NickServ')
    observer.expect(' 401 Watcher NickServ ')
    observer.send('ISON NickServ ChanServ BBSsvc_hub_bf09d1db')
    assert not observer.expect(' 303 Watcher ').split(' 303 Watcher ', 1)[1].lstrip(':').strip(), 'Disabled services introduced a helper or bot'
    assert not (run / 'hub.sqlite').exists(), 'Disabled services opened the registration database'
    print('PASS: omitted Enabled keeps matching services host inactive without a database or helper', flush=True)
    observer.send('OPER testop servicetest')
    observer.expect(' 381 ')
    hub.config_path.write_text(hub.config_path.read_text().replace('[EmbeddedServices]\n', '[EmbeddedServices]\nEnabled=true\n'))
    observer.send('REHASH')
    observer.expect(' 382 ')
    for _ in range(20):
        observer.send('WHOIS NickServ')
        try:
            observer.expect(' 311 Watcher NickServ ', timeout=1)
            break
        except AssertionError:
            time.sleep(.2)
    else:
        raise AssertionError('Central services did not start')
    relay.start([(hub, True, True), (home, False, False), (edge, False, False)])
    home.start([(relay, True, True)])
    edge.start([(relay, True, True)])
    owner = Client(home, 'Alice')
    outsider = Client(edge, 'Visitor')
    time.sleep(2)
    # Detailed help must work before any BBS identification, through the
    # ordinary client aliases and the actual legacy relay to ChanServ.
    owner.send('CS HELP REGISTER')
    owner.expect('Syntax: /cs REGISTER #channel')
    owner.expect('End of ChanServ HELP REGISTER.')
    time.sleep(1.1)
    owner.send('NS HELP IDENTIFY')
    owner.expect('Syntax: /msg NickServ@home.test IDENTIFY')
    owner.expect('End of NickServ HELP IDENTIFY.')
    time.sleep(1.1)
    owner.send('MS HELP READ')
    owner.expect('READ receives AND CONSUMES')
    owner.expect('End of MemoServ HELP READ.')
    time.sleep(1.1)
    owner.send('PRIVMSG MemoServ@edge.test :HELP SEND')
    owner.expect('Syntax: /msg MemoServ@edge.test SEND')
    owner.expect('End of MemoServ HELP SEND.')
    print('PASS: detailed alias and qualified help is public before identification, including ChanServ through a legacy relay', flush=True)
    owner.service('NickServ', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+HOME')
    print('PASS: native BBS identification through local NickServ', flush=True)
    owner.service('NickServ', 'REGISTER', 'Nickname registered to your network identity')
    print('PASS: central registration through legacy relay', flush=True)
    owner.send('JOIN #services-test')
    owner.expect(' 366 ')
    owner.service('ChanServ', 'REGISTER #services-test Services test', 'Channel registered to your account')
    print('PASS: ChanServ registration through legacy relay', flush=True)
    owner.service('NickServ', 'INFO Alice', 'Last used: ')
    owner.service('ChanServ', 'INFO #services-test', 'Last used: ')
    print('PASS: last-used timestamps are available through a legacy relay', flush=True)
    identified = outsider.service('NickServ@home.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+HOME')
    assert ' NOTICE Visitor :' in identified and '[NickServ@home.test]' in identified
    outsider.expect('/msg NickServ@home.test <command>')
    print('PASS: remote BBS identification through legacy relay', flush=True)
    status = outsider.service('MemoServ@home.test', 'STATUS', 'No telegrams waiting')
    assert ' NOTICE Visitor :' in status and '[MemoServ@home.test]' in status
    outsider.expect('/msg MemoServ@home.test <command>')
    print('PASS: remote BBS notices identify the server and explain qualified replies', flush=True)
    outsider.send('JOIN #services-test')
    outsider.expect(' 366 ')
    outsider.expect('MODE #services-test +o Visitor')
    print('PASS: automatic ops follow the same BBS user on a different server', flush=True)
    outsider.service('MemoServ', 'SEND Alice Services-network-telegram', 'Telegram delivered')
    owner.service('MemoServ', 'STATUS', 'You have telegrams waiting')
    owner.service('MemoServ', 'READ', 'Services-network-telegram')
    owner.service('MemoServ', 'STATUS', 'No telegrams waiting')
    print('PASS: cross-server telegram sending, checking, and consuming read', flush=True)
    outsider.service('MemoServ', 'SEND Some.User+EDGE Qualified-account-telegram', 'Telegram delivered')
    print('PASS: qualified telegram recipient without prior services identification', flush=True)
    wrong = Client(edge, 'WrongAccount')
    wrong.service('NickServ', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+EDGE')
    wrong.service('MemoServ', 'READ', 'Qualified-account-telegram')
    wrong.service('NickServ', 'GHOST Alice', 'must belong to your account')
    wrong.send('WHOIS Alice')
    wrong.expect(' 311 WrongAccount Alice ')
    print('PASS: same alias on a different BBS cannot ghost the owner', flush=True)
    password_ok = Client(home, 'PasswordLogin', 'TESTPASS', 'Some.User')
    password_ok.send('WHOIS PasswordLogin')
    assert ' Some.User+HOME ' in password_ok.expect(' 311 PasswordLogin PasswordLogin ')
    password_bad = Client(home, 'WrongPassword', 'wrong', 'Some.User')
    password_bad.send('WHOIS WrongPassword')
    assert ' ~' in password_bad.expect(' 311 WrongPassword WrongPassword ')
    print('PASS: USER/PASS shares authentication and a wrong password retains ~', flush=True)
    owner.service('ChanServ', 'ACCESS #services-test ADD Some.User+EDGE OP', 'Access list updated')
    wrong.send('JOIN #services-test')
    wrong.expect(' 366 ')
    wrong.expect('MODE #services-test +o WrongAccount')
    owner.send('NICK RenamedOwner')
    owner.expect(' NICK RenamedOwner')
    owner.nick = 'RenamedOwner'
    owner.service('ChanServ', 'SET #services-test MLOCK +m', 'Channel settings updated')
    owner.send('MODE #services-test -m')
    owner.expect('MODE #services-test +m')
    owner.service('ChanServ', 'TOPIC #services-test Account-owned topic', 'Topic change requested')
    wrong.expect('TOPIC #services-test :Account-owned topic')
    print('PASS: account access, nickname changes, mode locks, and topics', flush=True)
    # Rename the owner away, then use its other authenticated session to reclaim
    # an unauthenticated connection occupying the registered nickname.
    ghost = Client(edge, 'Alice')
    outsider.service('NickServ', 'GHOST Alice', 'Ghost session removal requested')
    try:
        ghost.expect('Nickname reclaimed', timeout=5)
    except AssertionError as error:
        if 'Connection closed' not in str(error):
            raise
    print('PASS: GHOST reclaims a nickname through a legacy relay', flush=True)
    if not args.skip_grace:
        trespasser = Client(edge, 'Alice')
        trespasser.service('NickServ', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+EDGE')
        line = trespasser.expect(' NICK Guest', timeout=70)
        trespasser.nick = line.split(' NICK ', 1)[1].lstrip(':')
        print('PASS: wrong-account nickname renamed after grace despite periodic claims', flush=True)
    if args.single_login_source:
        owner.service('NickServ@edge.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+EDGE')
        accounts = owner.service('NickServ', 'ACCOUNTS', 'Verified BBS logins:')
        assert 'Some.User+EDGE@edge.test (active)' in accounts and 'Some.User+HOME@home.test' not in accounts
        owner.service('NickServ', 'LINK Some.User+HOME Some.User+EDGE', 'LINK requires updated services')
        owner.service('NickServ@home.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+HOME')
        time.sleep(2)
        accounts = owner.service('NickServ', 'ACCOUNTS', 'Verified BBS logins:')
        assert 'Some.User+HOME@home.test (active)' in accounts and 'Some.User+EDGE@edge.test' not in accounts
        print('PASS: old BBS helper safely retains single-login behavior and refuses LINK', flush=True)
    else:
        owner.service('NickServ@edge.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+EDGE')
        accounts = owner.service('NickServ', 'ACCOUNTS', 'Verified BBS logins:')
        assert 'Some.User+HOME@home.test' in accounts and 'Some.User+EDGE@edge.test (active)' in accounts
        owner.service('NickServ', 'REGISTER', 'Nickname registered to your network identity')
        preview = owner.service('NickServ', 'LINK Some.User+HOME Some.User+EDGE', 'Confirm within 5 minutes: LINK CONFIRM ')
        code = preview.split('LINK CONFIRM ', 1)[1]
        owner.service('NickServ', 'LINK CONFIRM ' + code, 'Network identities linked')
        wrong.service('ChanServ', 'SET #services-test KEEPTOPIC ON', 'Channel settings updated')
        print('PASS: dual BBS login and confirmed identity merge grant equal founder rights through legacy relay', flush=True)
    hub.process.terminate()
    assert hub.process.wait(timeout=10) == 0, 'Central IRCd failed during restart'
    hub.log.close()
    # Use another test port to avoid a native listener's TIME_WAIT rebinding
    # limitation. The same authority and SQLite database reconnect outward.
    hub.port = 19874
    hub.start([(relay, True, True)])
    observer = Client(hub, 'Watcher')
    time.sleep(8)
    owner.service('NickServ', 'INFO Alice', 'Registered network identity: ALICE')
    owner.service('NickServ', 'INFO Alice', 'Last used: ')
    outsider.service('ChanServ', 'INFO #services-test', 'Last used: ')
    outsider.service('ChanServ', 'TOPIC #services-test After central restart', 'Topic change requested')
    wrong.expect('TOPIC #services-test :After central restart')
    print('PASS: SQLite ownership and foreign-BBS sessions recover after authority restart', flush=True)
    if not args.single_login_source:
        accounts = owner.service('NickServ', 'ACCOUNTS', 'Verified BBS logins:')
        assert 'Some.User+HOME@home.test' in accounts and 'Some.User+EDGE@edge.test (active)' in accounts
        owner.service('NickServ', 'SET Alice MEMOACCOUNT Some.User+EDGE', 'Telegram destination updated')
        outsider.service('MemoServ', 'SEND Alice Linked-destination-telegram', 'Telegram delivered')
        owner.service('MemoServ@home.test', 'STATUS', 'No telegrams waiting')
        group_memo(owner, 'MemoServ@edge.test', 'READ', 'Linked-destination-telegram')
        owner.service('NickServ', 'SET RenamedOwner MEMOACCOUNT Some.User+HOME', 'Telegram destination updated')
        outsider.service('MemoServ', 'SEND RenamedOwner Default-home-mailbox', 'Telegram delivered')
        owner.service('MemoServ', 'STATUS', 'Telegram mailbox Some.User+HOME@home.test:')
        owner.expect('You have telegrams waiting')
        owner.expect('Checked ')
        group_memo(owner, 'MemoServ', 'READ', 'Default-home-mailbox')
        outsider.service('MemoServ', 'SEND RenamedOwner Automatic-default-notification', 'Telegram delivered')
        owner.service('NickServ@edge.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+EDGE')
        owner.expect('Telegram mailbox Some.User+HOME@home.test:')
        owner.expect('You have telegrams waiting')
        owner.expect('Checked ')
        group_memo(owner, 'MemoServ', 'READ', 'Automatic-default-notification')
        outsider.service('MemoServ@home.test', 'SEND Alice Explicit-home-destination', 'Telegram delivered')
        group_memo(owner, 'MemoServ@home.test', 'READ', 'Explicit-home-destination')
        outsider.service('MemoServ@edge.test', 'SEND Alice Explicit-edge-destination', 'Telegram delivered')
        group_memo(owner, 'MemoServ@edge.test', 'READ', 'Explicit-edge-destination')
        outsider.service('MemoServ@hub.test', 'SEND Alice Unlinked-destination', 'no linked account on hub.test')
        outsider.service('MemoServ@edge.test', 'SEND Some.User+HOME Conflicting-destination', 'destination account belongs to home.test')
        outsider.service('MemoServ', 'SEND RenamedOwner Shared-home-mailbox', 'Telegram delivered')
        outsider.service('MemoServ@edge.test', 'SEND RenamedOwner Shared-edge-mailbox', 'Telegram delivered')
        owner.service('NickServ@home.test', 'LOGOUT', 'Logged out of home.test')
        accounts = owner.service('NickServ', 'ACCOUNTS', 'Verified BBS logins:')
        assert 'Some.User+EDGE@edge.test' in accounts and 'Some.User+HOME@home.test' not in accounts
        owner.service('MemoServ', 'READ', 'Shared-home-mailbox')
        owner.expect('Shared-edge-mailbox')
        owner.expect('Checked 2 telegram mailboxes')
        group_memo(owner, 'MemoServ@home.test', 'STATUS', 'No telegrams waiting')
        owner.service('NickServ@home.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+HOME')
        print('PASS: one group login reads all associated BBS mailboxes; configured and explicit delivery destinations remain distinct', flush=True)
        owner.service('NickServ@edge.test', 'LOGOUT', 'Logged out of edge.test')
        owner.send('WHOIS RenamedOwner')
        assert ' Some.User+HOME ' in owner.expect(' 311 RenamedOwner RenamedOwner ')
        group_memo(owner, 'MemoServ@edge.test', 'READ', 'No telegrams waiting')
        owner.service('ChanServ', 'SET #services-test KEEPTOPIC OFF', 'Channel settings updated')
        print('PASS: authority restart restores both logins; telegram routing and qualified reads retain their BBS selection', flush=True)
        owner.service('NickServ', 'UNLINK Some.User+EDGE', 'Retarget MEMOACCOUNT before unlinking:')
        owner.service('NickServ', 'SET Alice MEMOACCOUNT Some.User+HOME', 'Telegram destination updated')
        owner.service('NickServ', 'SET RenamedOwner MEMOACCOUNT Some.User+HOME', 'Telegram destination updated')
        owner.service('NickServ', 'UNLINK Some.User+EDGE', 'BBS account unlinked into an empty identity')
        owner.service('MemoServ@edge.test', 'READ', 'No account in your authenticated network identities')
        wrong.service('ChanServ', 'SET #services-test KEEPTOPIC ON', 'Identify to an account with access')
        owner.service('NickServ', 'LOGOUT ALL', 'All BBS logins removed')
        owner.send('WHOIS RenamedOwner')
        assert ' ~' in owner.expect(' 311 RenamedOwner RenamedOwner ')
        owner.service('ChanServ', 'SET #services-test KEEPTOPIC ON', 'Identify to an account with access')
        print('PASS: unlink retains registrations, revokes detached founder rights, and LOGOUT ALL restores anonymous identity', flush=True)
    # A failed server password must not shut down embedded services. An
    # authenticated external services server takes ownership of its namespace.
    for password in ['wrong', 'servicetest']:
        external = socket.create_connection((hub.bind, hub.port), timeout=5)
        clients.append(SimpleNamespace(sock=external))
        external.sendall(('PASS ' + password + '\r\nSERVER 127.0.0.1 1 :External services test\r\n').encode())
        time.sleep(2)
        observer.send('WHOIS NickServ')
        observer.expect(' 311 Watcher NickServ ' if password == 'wrong' else ' 401 Watcher NickServ ')
    print('PASS: external services takeover requires valid server authentication', flush=True)
finally:
    for client in clients:
        try:
            client.sock.close()
        except OSError:
            pass
    failures = []
    for server in reversed(servers):
        before = server.process.poll()
        if before is None:
            server.process.terminate()
        try:
            status = server.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.process.kill()
            status = server.process.wait(timeout=5)
            failures.append(server.name + ' did not terminate promptly')
        server.log.close()
        print(f'{server.name}: exit={status}, log={server.log_path}', flush=True)
        if before is not None or status not in (0, -signal.SIGTERM):
            failures.append(f'{server.name} unexpected exit {status}')
    print('Logs:', run, flush=True)
    if failures:
        raise AssertionError('; '.join(failures))
